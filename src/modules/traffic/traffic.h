#ifndef TRAFFIC_H
#define TRAFFIC_H

#include "../map/map.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * Descrição: Definição das estruturas de dados do subsistema de semáforos,
 * representando os estados de trânsito nos cruzamentos e as primitivas de
 * sincronização para os veículos.
 * Autor: Paulo e Leôncio Ferreira
 */

#define INTERSECTION_CAPACITY 2

typedef enum { LIGHT_HORIZ_GREEN, LIGHT_VERT_GREEN } LightState;

typedef struct TrafficLight {
  int id;
  int row;
  int col;
  LightState state;
  int toggle_ticks;

  bool priority_active;
  char priority_dir;

  pthread_mutex_t mutex;

  sem_t horiz_sem;
  sem_t vert_sem;

  int horiz_waiters;
  int vert_waiters;

  sem_t capacity_sem;
  int capacity_waiters;

  struct TrafficLight *base_light;

} TrafficLight;

void traffic_init(const Map *map, int default_toggle_ticks);
void traffic_start(void);
void traffic_stop(void);
void traffic_destroy(void);

void traffic_wait_for_green(int row, int col, char vehicle_dir);
bool traffic_is_green(int row, int col, char vehicle_dir);
bool traffic_is_safe_to_enter(int row, int col, char vehicle_dir,
                              uint64_t current_tick);

void traffic_request_priority(int row, int col, char direction);
void traffic_release_priority(int row, int col);
bool traffic_is_priority_active(void);

bool traffic_try_enter_capacity(int row, int col);
void traffic_release_capacity(int row, int col);

void traffic_enter_intersection(int row, int col);
void traffic_leave_intersection(int row, int col);

#endif