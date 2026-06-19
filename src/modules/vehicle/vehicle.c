#include "vehicle.h"
#include "../clock/clock.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
/**
 * Descrição: Implementação do ciclo de vida e lógicas de exclusão mútua
 * (impenetratividade) das threads de Veículos na simulação.
 * Autor: Leôncio Ferreira
 */

static void calculate_next_position(char direction, int current_row,
                                    int current_col, int *next_row,
                                    int *next_col) {
  *next_row = current_row;
  *next_col = current_col;

  switch (direction) {
  case 'N':
  case '^':
    (*next_row)--;
    break;
  case 'S':
  case 'v':
    (*next_row)++;
    break;
  case 'L':
  case '>':
    (*next_col)++;
    break;
  case 'O':
  case '<':
    (*next_col)--;
    break;
  }
}

static bool is_within_map_bounds(const Map *map, int row, int col) {
  return (row >= 0 && row < map->rows && col >= 0 && col < map->columns);
}

static uint64_t wait_for_next_tick() {
  pthread_mutex_lock(&clock_mutex);
  pthread_cond_wait(&clock_cond, &clock_mutex);
  uint64_t current_tick = global_tick;
  pthread_mutex_unlock(&clock_mutex);
  return current_tick;
}

void *vehicle_lifecycle(void *arg) {
  Vehicle *vehicle = (Vehicle *)arg;
  Map *map = vehicle->map_ref;

  pthread_mutex_lock(&map->cell_grid[vehicle->row][vehicle->col].mutex);

  while (true) {
    uint64_t current_tick = wait_for_next_tick();

    if (current_tick % vehicle->speed_ticks != 0) {
      continue;
    }
    char current_direction =
        map->cell_grid[vehicle->row][vehicle->col].direction;
    int next_row, next_col;

    calculate_next_position(current_direction, vehicle->row, vehicle->col,
                            &next_row, &next_col);

    if (is_within_map_bounds(map, next_row, next_col)) {
      pthread_mutex_lock(&map->cell_grid[next_row][next_col].mutex);
      pthread_mutex_unlock(&map->cell_grid[vehicle->row][vehicle->col].mutex);

      vehicle->row = next_row;
      vehicle->col = next_col;
    } else {
      pthread_mutex_unlock(&map->cell_grid[vehicle->row][vehicle->col].mutex);
      break;
    }
  }
  return NULL;
}

Vehicle *vehicle_create_and_start(int id, int start_row, int start_col,
                                  int speed_ticks, VehicleType type, Map *map) {
  Vehicle *vehicle = (Vehicle *)malloc(sizeof(Vehicle));

  if (!vehicle)
    return NULL;

  vehicle->id = id;
  vehicle->row = start_row;
  vehicle->col = start_col;
  vehicle->speed_ticks = speed_ticks;
  vehicle->type = type;
  vehicle->map_ref = map;

  pthread_create(&vehicle->thread_id, NULL, vehicle_lifecycle, (void *)vehicle);
  return vehicle;
}

void vehicle_destroy(Vehicle *vehicle) {
  pthread_join(vehicle->thread_id, NULL);
  free(vehicle);
}
