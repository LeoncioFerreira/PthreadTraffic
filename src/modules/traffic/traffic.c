#include "traffic.h"
#include "../clock/clock.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Descrição: Implementação do gerenciamento de semáforos e controle de
 * capacidade de cruzamentos. Inclui lógica para agrupar células de um mesmo
 * cruzamento 2x2 sob uma única célula base.
 * Autor: Paulo e Leôncio Ferreira
 */

static TrafficLight *lights = NULL;
static int num_lights = 0;
static pthread_t manager_thread;

static atomic_bool keep_traffic_running = false;

/* Forward declarations */
static LightState compute_light_state(const TrafficLight *tl, uint64_t tick);
static TrafficLight *get_light_at(int row, int col);

static bool is_horizontal_dir(char dir) {
  return (dir == 'L' || dir == '>' || dir == 'O' || dir == '<');
}

static bool is_vertical_dir(char dir) {
  return (dir == 'N' || dir == '^' || dir == 'S' || dir == 'v');
}

static bool is_allowed(LightState state, char dir) {
  if (state == LIGHT_HORIZ_GREEN && is_horizontal_dir(dir))
    return true;
  if (state == LIGHT_VERT_GREEN && is_vertical_dir(dir))
    return true;
  return false;
}

static void semaphore_broadcast_green(TrafficLight *tl, LightState new_state) {
  if (new_state == LIGHT_HORIZ_GREEN) {
    for (int w = 0; w < tl->horiz_waiters; w++) {
      sem_post(&tl->horiz_sem);
    }
  } else {
    for (int w = 0; w < tl->vert_waiters; w++) {
      sem_post(&tl->vert_sem);
    }
  }
}

static void semaphore_broadcast_all(TrafficLight *tl) {
  for (int w = 0; w < tl->horiz_waiters; w++) {
    sem_post(&tl->horiz_sem);
  }
  for (int w = 0; w < tl->vert_waiters; w++) {
    sem_post(&tl->vert_sem);
  }
}

static void *traffic_manager_routine(void *arg) {
  (void)arg;
  uint64_t last_processed_tick = 0;

  while (atomic_load(&keep_traffic_running)) {
    pthread_mutex_lock(&clock_mutex);
    pthread_cond_wait(&clock_cond, &clock_mutex);
    uint64_t current_tick = global_tick;
    pthread_mutex_unlock(&clock_mutex);

    if (!atomic_load(&keep_traffic_running))
      break;

    if (current_tick == last_processed_tick)
      continue;
    last_processed_tick = current_tick;

    for (int i = 0; i < num_lights; i++) {
      TrafficLight *tl = &lights[i];

      int base_id = (tl->base_light != NULL) ? tl->base_light->id : tl->id;

      if ((current_tick + (uint64_t)base_id) % (uint64_t)tl->toggle_ticks ==
          0) {
        pthread_mutex_lock(&tl->mutex);

        LightState new_state = compute_light_state(tl, current_tick);
        tl->state = new_state;

        semaphore_broadcast_green(tl, new_state);

        pthread_mutex_unlock(&tl->mutex);
      }
    }
  }
  return NULL;
}

static TrafficLight *get_light_at(int row, int col) {
  for (int i = 0; i < num_lights; i++) {
    if (lights[i].row == row && lights[i].col == col)
      return &lights[i];
  }
  return NULL;
}

void traffic_wait_for_green(int row, int col, char vehicle_dir) {
  TrafficLight *tl = get_light_at(row, col);
  if (!tl)
    return;

  bool is_horiz = is_horizontal_dir(vehicle_dir);
  sem_t *sem = is_horiz ? &tl->horiz_sem : &tl->vert_sem;

  while (atomic_load(&keep_traffic_running)) {
    pthread_mutex_lock(&tl->mutex);

    if (is_allowed(tl->state, vehicle_dir) ||
        !atomic_load(&keep_traffic_running)) {
      pthread_mutex_unlock(&tl->mutex);
      return;
    }

    if (is_horiz) {
      tl->horiz_waiters++;
    } else {
      tl->vert_waiters++;
    }

    pthread_mutex_unlock(&tl->mutex);

    sem_wait(sem);

    pthread_mutex_lock(&tl->mutex);
    if (is_horiz) {
      tl->horiz_waiters--;
    } else {
      tl->vert_waiters--;
    }
    bool still_allowed = is_allowed(tl->state, vehicle_dir);
    pthread_mutex_unlock(&tl->mutex);

    if (still_allowed)
      return;
  }
}

static LightState compute_light_state(const TrafficLight *tl, uint64_t tick) {
  if (tl->priority_active) {
    if (is_horizontal_dir(tl->priority_dir))
      return LIGHT_HORIZ_GREEN;
    return LIGHT_VERT_GREEN;
  }

  int base_id = (tl->base_light != NULL) ? tl->base_light->id : tl->id;
  uint64_t phase =
      ((tick + (uint64_t)base_id) / (uint64_t)tl->toggle_ticks) % 2;
  return (phase == 0) ? LIGHT_HORIZ_GREEN : LIGHT_VERT_GREEN;
}

bool traffic_is_green(int row, int col, char vehicle_dir) {
  TrafficLight *tl = get_light_at(row, col);
  if (!tl)
    return true;

  pthread_mutex_lock(&clock_mutex);
  uint64_t tick = global_tick;
  pthread_mutex_unlock(&clock_mutex);

  pthread_mutex_lock(&tl->mutex);
  LightState state = compute_light_state(tl, tick);
  bool allowed = is_allowed(state, vehicle_dir);
  pthread_mutex_unlock(&tl->mutex);

  return allowed;
}

bool traffic_is_safe_to_enter(int row, int col, char vehicle_dir,
                              uint64_t current_tick) {
  TrafficLight *tl = get_light_at(row, col);
  if (!tl)
    return true;

  pthread_mutex_lock(&tl->mutex);
  LightState state = compute_light_state(tl, current_tick);
  bool allowed = is_allowed(state, vehicle_dir);
  pthread_mutex_unlock(&tl->mutex);

  return allowed;
}

void traffic_init(const Map *map, int default_toggle_ticks) {
  if (default_toggle_ticks <= 0) {
    fprintf(stderr, "[TRAFFIC] Erro: default_toggle_ticks deve ser > 0.\n");
    exit(EXIT_FAILURE);
  }

  num_lights = 0;
  for (int row = 0; row < map->rows; row++)
    for (int col = 0; col < map->columns; col++)
      if (map->cell_grid[row][col].type == INTERSECTION)
        num_lights++;

  lights = (TrafficLight *)malloc((size_t)num_lights * sizeof(TrafficLight));
  if (lights == NULL) {
    fprintf(
        stderr,
        "[TRAFFIC] Erro crítico: Memória insuficiente para os semáforos.\n");
    exit(EXIT_FAILURE);
  }

  int index = 0;
  for (int row = 0; row < map->rows; row++) {
    for (int col = 0; col < map->columns; col++) {
      if (map->cell_grid[row][col].type == INTERSECTION) {
        TrafficLight *tl = &lights[index];

        tl->id = index;
        tl->row = row;
        tl->col = col;
        tl->state = LIGHT_HORIZ_GREEN;
        tl->toggle_ticks = default_toggle_ticks;
        tl->priority_active = false;
        tl->priority_dir = ' ';
        tl->horiz_waiters = 0;
        tl->vert_waiters = 0;
        tl->capacity_waiters = 0;
        tl->base_light = tl;

        pthread_mutex_init(&tl->mutex, NULL);
        sem_init(&tl->horiz_sem, 0, 0);
        sem_init(&tl->vert_sem, 0, 0);
        sem_init(&tl->capacity_sem, 0, INTERSECTION_CAPACITY);

        index++;
      }
    }
  }

  for (int i = 0; i < num_lights; i++) {
    TrafficLight *tl = &lights[i];
    int base_r = tl->row;
    int base_c = tl->col;

    if (base_r > 0 && map->cell_grid[base_r - 1][base_c].type == INTERSECTION) {
      base_r--;
    }
    if (base_c > 0 && map->cell_grid[base_r][base_c - 1].type == INTERSECTION) {
      base_c--;
    }

    TrafficLight *base = get_light_at(base_r, base_c);
    if (base != NULL) {
      tl->base_light = base;
    }
  }
}

void traffic_start(void) {
  atomic_store(&keep_traffic_running, true);

  if (pthread_create(&manager_thread, NULL, traffic_manager_routine, NULL) !=
      0) {
    fprintf(stderr, "[TRAFFIC] Erro: falha ao criar thread do gerenciador.\n");
    traffic_destroy();
    exit(EXIT_FAILURE);
  }
}

void traffic_stop(void) {
  atomic_store(&keep_traffic_running, false);

  pthread_mutex_lock(&clock_mutex);
  pthread_cond_broadcast(&clock_cond);
  pthread_mutex_unlock(&clock_mutex);

  for (int i = 0; i < num_lights; i++) {
    pthread_mutex_lock(&lights[i].mutex);
    semaphore_broadcast_all(&lights[i]);

    for (int w = 0; w < lights[i].capacity_waiters; w++) {
      sem_post(&lights[i].capacity_sem);
    }

    pthread_mutex_unlock(&lights[i].mutex);
  }

  pthread_join(manager_thread, NULL);
}

void traffic_destroy(void) {
  if (lights == NULL)
    return;

  atomic_store(&keep_traffic_running, false);

  for (int i = 0; i < num_lights; i++) {
    pthread_mutex_destroy(&lights[i].mutex);
    sem_destroy(&lights[i].horiz_sem);
    sem_destroy(&lights[i].vert_sem);
    sem_destroy(&lights[i].capacity_sem);
  }

  free(lights);
  lights = NULL;
  num_lights = 0;
}

void traffic_request_priority(int row, int col, char direction) {
  TrafficLight *tl = get_light_at(row, col);
  if (!tl)
    return;

  pthread_mutex_lock(&tl->mutex);
  tl->priority_active = true;
  tl->priority_dir = direction;

  LightState new_state = compute_light_state(tl, 0);
  tl->state = new_state;
  semaphore_broadcast_green(tl, new_state);
  pthread_mutex_unlock(&tl->mutex);
}

void traffic_release_priority(int row, int col) {
  TrafficLight *tl = get_light_at(row, col);
  if (!tl)
    return;

  pthread_mutex_lock(&clock_mutex);
  uint64_t tick = global_tick;
  pthread_mutex_unlock(&clock_mutex);

  pthread_mutex_lock(&tl->mutex);
  tl->priority_active = false;

  LightState new_state = compute_light_state(tl, tick);
  tl->state = new_state;
  semaphore_broadcast_green(tl, new_state);
  pthread_mutex_unlock(&tl->mutex);
}

bool traffic_is_priority_active(void) {
  if (!lights)
    return false;
  for (int i = 0; i < num_lights; i++) {
    if (lights[i].priority_active)
      return true;
  }
  return false;
}

bool traffic_try_enter_capacity(int row, int col) {
  TrafficLight *tl = get_light_at(row, col);
  if (!tl)
    return true;

  /* Redireciona o consumo do semáforo para a célula base do cruzamento */
  TrafficLight *base = tl->base_light ? tl->base_light : tl;
  return (sem_trywait(&base->capacity_sem) == 0);
}

void traffic_release_capacity(int row, int col) {
  TrafficLight *tl = get_light_at(row, col);
  if (!tl)
    return;

  TrafficLight *base = tl->base_light ? tl->base_light : tl;
  sem_post(&base->capacity_sem);
}

void traffic_enter_intersection(int row, int col) {
  TrafficLight *tl = get_light_at(row, col);
  if (!tl)
    return;

  TrafficLight *base = tl->base_light ? tl->base_light : tl;

  pthread_mutex_lock(&base->mutex);
  if (!atomic_load(&keep_traffic_running)) {
    pthread_mutex_unlock(&base->mutex);
    return;
  }
  base->capacity_waiters++;
  pthread_mutex_unlock(&base->mutex);

  sem_wait(&base->capacity_sem);

  pthread_mutex_lock(&base->mutex);
  base->capacity_waiters--;
  pthread_mutex_unlock(&base->mutex);
}

void traffic_leave_intersection(int row, int col) {
  traffic_release_capacity(row, col);
}