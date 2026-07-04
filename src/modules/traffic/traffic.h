#ifndef TRAFFIC_H
#define TRAFFIC_H

#include "../map/map.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * Descrição: Definição das estruturas de dados do subsistema de semáforos,
 * representando os estados de trânsito nos cruzamentos e as primitivas de
 * sincronização para os veículos.
 * Autor: Paulo
 */

typedef enum { LIGHT_HORIZ_GREEN, LIGHT_VERT_GREEN } LightState;

typedef struct {
  int id;
  int row;
  int col;
  LightState state;
  int toggle_ticks;

  pthread_mutex_t mutex;
  pthread_cond_t cond;
} TrafficLight;

void traffic_init(const Map *map, int default_toggle_ticks);

void traffic_start(void);

void traffic_stop(void);

void traffic_destroy(void);

void traffic_wait_for_green(int row, int col, char vehicle_dir);

bool traffic_is_green(int row, int col, char vehicle_dir);

bool traffic_is_safe_to_enter(int row, int col, char vehicle_dir,
                              uint64_t current_tick);

#endif
