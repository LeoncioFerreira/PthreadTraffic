#include "traffic.h"
#include "../clock/clock.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * Descrição: Esse arquivo implementa a thread que gerencia os semáforos,
 * alternando os sinais entre as direções ao ritmo do relógio global. Também
 * fornece mecanismos  para barrar os carros no sinal vermelho e acordá-los com
 * segurança quando o sinal abrir. Autor: Paulo
 */
static TrafficLight *lights = NULL;
static int num_lights = 0;
static pthread_t manager_thread;
static volatile bool keep_traffic_running = false;

// por que a dir do carro pode ser símbolo e direção ? tipo ser L ou <
static bool is_allowed(LightState state, char dir) {
  if (state == LIGHT_HORIZ_GREEN &&
      (dir == 'L' || dir == '>' || dir == 'O' || dir == '<')) {
    return true;
  }

  if (state == LIGHT_VERT_GREEN &&
      (dir == 'N' || dir == '^' || dir == 'S' || dir == 'v')) {
    return true;
  }

  return false;
}

static void *traffic_manager_routine(void *arg) {
  (void)arg;

  uint64_t last_processed_tick = 0;
  while (keep_traffic_running) {

    pthread_mutex_lock(&clock_mutex);
    pthread_cond_wait(&clock_cond, &clock_mutex);
    uint64_t current_tick = global_tick;
    pthread_mutex_unlock(&clock_mutex);

    if (!keep_traffic_running) {
      break;
    }

    if (current_tick == last_processed_tick) {
      continue;
    }
    last_processed_tick = current_tick;

    for (int i = 0; i < num_lights; i++) {
      TrafficLight *tl = &lights[i];

      if (current_tick % (uint64_t)tl->toggle_ticks == 0) {
        pthread_mutex_lock(&tl->mutex);

        if (tl->state == LIGHT_HORIZ_GREEN) {

          tl->state = LIGHT_VERT_GREEN;
        } else {
          tl->state = LIGHT_HORIZ_GREEN;
        }
        pthread_cond_broadcast(&tl->cond);
        pthread_mutex_unlock(&tl->mutex);
      }
    }
  }
  return NULL;
}

static TrafficLight *get_light_at(int row, int col) {

  for (int i = 0; i < num_lights; i++) {
    if (lights[i].row == row && lights[i].col == col) {
      return &lights[i];
    }
  }
  return NULL;
}

void traffic_wait_for_green(int row, int col, char vehicle_dir) {
  TrafficLight *tl = get_light_at(row, col);
  if (!tl) {
    return;
  }

  pthread_mutex_lock(&tl->mutex);

  while (!is_allowed(tl->state, vehicle_dir) && keep_traffic_running) {
    pthread_cond_wait(&tl->cond, &tl->mutex);
  }
  pthread_mutex_unlock(&tl->mutex);
}
bool traffic_is_green(int row, int col, char vehicle_dir) {
  TrafficLight *tl = get_light_at(row, col);
  if (!tl) {
    return true;
  }
  pthread_mutex_lock(&tl->mutex);
  bool allowed = is_allowed(tl->state, vehicle_dir);
  pthread_mutex_unlock(&tl->mutex);

  return allowed;
}

void traffic_init(const Map *map, int default_toggle_ticks) {
  if (default_toggle_ticks <= 0) {
    fprintf(stderr, "[TRAFFIC] Erro: default_toggle_ticks deve ser > 0.\n");
    exit(EXIT_FAILURE);
  }
  num_lights = 0;
  for (int row = 0; row < map->rows; row++) {
    for (int colum = 0; colum < map->columns; colum++) {
      if (map->cell_grid[row][colum].type == INTERSECTION) {
        num_lights++;
      }
    }
  }
  lights = (TrafficLight *)malloc((size_t)num_lights * sizeof(TrafficLight));
  if (lights == NULL) {
    fprintf(
        stderr,
        "[TRAFFIC] Erro crítico: Memória insuficiente para os semáforos.\n");
    exit(EXIT_FAILURE);
  }

  int index = 0;

  for (int row = 0; row < map->rows; row++) {
    for (int colum = 0; colum < map->columns; colum++) {
      if (map->cell_grid[row][colum].type == INTERSECTION) {
        lights[index].id = index;
        lights[index].row = row;
        lights[index].col = colum;
        lights[index].state = LIGHT_HORIZ_GREEN;
        lights[index].toggle_ticks = default_toggle_ticks;
        pthread_mutex_init(&lights[index].mutex, NULL);
        pthread_cond_init(&lights[index].cond, NULL);
        index++;
      }
    }
  }
}

void traffic_start(void) {
  keep_traffic_running = true;

  if (pthread_create(&manager_thread, NULL, traffic_manager_routine, NULL) !=
      0) {
    fprintf(stderr, "[TRAFFIC] Erro: falha ao criar thread do gerenciador.\n");
    traffic_destroy();
    exit(EXIT_FAILURE);
  }
}

void traffic_stop(void) {
  keep_traffic_running = false;

  pthread_mutex_lock(&clock_mutex);
  pthread_cond_broadcast(&clock_cond);
  pthread_mutex_unlock(&clock_mutex);

  for (int i = 0; i < num_lights; i++) {
    pthread_mutex_lock(&lights[i].mutex);
    pthread_cond_broadcast(&lights[i].cond);
    pthread_mutex_unlock(&lights[i].mutex);
  }

  pthread_join(manager_thread, NULL);
}

void traffic_destroy(void) {
  if (lights == NULL) {
    return;
  }

  for (int i = 0; i < num_lights; i++) {
    pthread_mutex_destroy(&lights[i].mutex);
    pthread_cond_destroy(&lights[i].cond);
  }
  free(lights);
  lights = NULL;
  num_lights = 0;
}
