#include "traffic.h"
#include "../clock/clock.h"
#include "../logger/logger.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Descrição: Esse arquivo implementa a thread que gerencia os semáforos,
 * alternando os sinais entre as direções ao ritmo do relógio global. Também
 * fornece mecanismos  para barrar os carros no sinal vermelho e acordá-los com
 * segurança quando o sinal abrir. Autor: Paulo e Leôncio Ferreira
 */

static TrafficLight *lights = NULL;
static int num_lights = 0;
static pthread_t manager_thread;

static atomic_bool keep_traffic_running = false;

// Declaração prévia (forward declaration) para uso na thread
static LightState compute_light_state(const TrafficLight *tl, uint64_t tick);

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
  while (atomic_load(&keep_traffic_running)) {

    pthread_mutex_lock(&clock_mutex);
    pthread_cond_wait(&clock_cond, &clock_mutex);
    uint64_t current_tick = global_tick;
    pthread_mutex_unlock(&clock_mutex);

    if (!atomic_load(&keep_traffic_running)) {
      break;
    }

    if (current_tick == last_processed_tick) {
      continue;
    }
    last_processed_tick = current_tick;

    for (int i = 0; i < num_lights; i++) {
      TrafficLight *tl = &lights[i];

      pthread_mutex_lock(&tl->mutex);

      LightState new_state = compute_light_state(tl, current_tick);
      bool changed = (new_state != tl->state);
      tl->state = new_state;

      if (changed) {
        // LOG CONCORRENTE DE ALTERAÇÃO DO SEMÁFORO
        logger_write(LOG_INFO,
                     "Semáforo %d em [%d][%d] alterou para estado: %s", tl->id,
                     tl->row, tl->col,
                     new_state == LIGHT_HORIZ_GREEN ? "VERDE HORIZONTAL"
                                                    : "VERDE VERTICAL");

        pthread_cond_broadcast(&tl->cond);
      }

      pthread_mutex_unlock(&tl->mutex);
    }
  }
  return NULL;
}

static TrafficLight *get_light_at(int row, int col) {
  //
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

  while (atomic_load(&keep_traffic_running)) {
    if (is_allowed(tl->state, vehicle_dir)) {
      break;
    }
    pthread_cond_wait(&tl->cond, &tl->mutex);
  }

  pthread_mutex_unlock(&tl->mutex);
}

// Calcula o estado do semáforo deterministicamente a partir do tick,
// sem depender da thread do semáforo (elimina race condition).
static LightState compute_light_state(const TrafficLight *tl, uint64_t tick) {
  // Prioridade da Ambulância, força a cor do sinal
  if (tl->priority_active) {
    if (tl->priority_dir == 'L' || tl->priority_dir == '>' ||
        tl->priority_dir == 'O' || tl->priority_dir == '<') {
      return LIGHT_HORIZ_GREEN;
    } else {
      return LIGHT_VERT_GREEN;
    }
  }
  // Comportamento normal do relógio com offset para dessincronizar
  uint64_t phase = ((tick + (uint64_t)tl->id) / (uint64_t)tl->toggle_ticks) % 2;
  return (phase == 0) ? LIGHT_HORIZ_GREEN : LIGHT_VERT_GREEN;
}

bool traffic_is_green(int row, int col, char vehicle_dir) {
  TrafficLight *tl = get_light_at(row, col);
  if (!tl) {
    return true;
  }
  // Usa global_tick para o display (que não passa tick)
  pthread_mutex_lock(&clock_mutex);
  uint64_t tick = global_tick;
  pthread_mutex_unlock(&clock_mutex);

  LightState state = compute_light_state(tl, tick);
  return is_allowed(state, vehicle_dir);
}

bool traffic_is_safe_to_enter(int row, int col, char vehicle_dir,
                              uint64_t current_tick) {
  TrafficLight *tl = get_light_at(row, col);
  if (!tl) {
    return true;
  }
  LightState state = compute_light_state(tl, current_tick);
  return is_allowed(state, vehicle_dir);
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
        lights[index].priority_active = false;
        lights[index].priority_dir = ' ';
        pthread_mutex_init(&lights[index].mutex, NULL);
        pthread_cond_init(&lights[index].cond, NULL);
        index++;
      }
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

void traffic_request_priority(int row, int col, char direction) {
  TrafficLight *tl = get_light_at(row, col);
  if (!tl)
    return;

  pthread_mutex_lock(&tl->mutex);
  tl->priority_active = true;
  tl->priority_dir = direction;

  tl->state = compute_light_state(tl, 0);
  pthread_cond_broadcast(&tl->cond);
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

  tl->state = compute_light_state(tl, tick);
  pthread_cond_broadcast(&tl->cond);
  pthread_mutex_unlock(&tl->mutex);
}

bool traffic_is_priority_active(void) {
  if (!lights)
    return false;

  for (int i = 0; i < num_lights; i++) {
    pthread_mutex_lock(&lights[i].mutex);
    bool priority_active = lights[i].priority_active;
    pthread_mutex_unlock(&lights[i].mutex);

    if (priority_active) {
      return true;
    }
  }
  return false;
}
