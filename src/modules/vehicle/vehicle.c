#include "vehicle.h"
#include "../clock/clock.h"
#include "vehicle_utils.h"
#include <stdlib.h>

/**
 * Descrição: Implementação do ciclo de vida das threads de veículos, das
 * lógicas de exclusão mútua na simulação e da estratégia anti-deadlock por meio
 * de Look-Ahead estendido.
 * Autores: Leôncio Ferreira e André Wesley.
 */

void *vehicle_lifecycle(void *arg) {
  Vehicle *vehicle = (Vehicle *)arg;
  Map *map = vehicle->map_ref;

  // Trava a célula inicial e se registra nela
  pthread_mutex_lock(&map->cell_grid[vehicle->row][vehicle->col].mutex);
  map->cell_grid[vehicle->row][vehicle->col].current_vehicle = vehicle;

  bool current_owns_exit_lock = false;
  int locked_exit_row = -1;
  int locked_exit_col = -1;

  char last_valid_direction =
      map->cell_grid[vehicle->row][vehicle->col].direction;
  if (last_valid_direction == ' ') {
    last_valid_direction = 'L';
  }

  char current_direction = last_valid_direction;

  while (clock_is_running()) {
    uint64_t current_tick = clock_wait_for_tick();

    if (!clock_is_running()) {
      vehicle_exit_map_cleanup(vehicle, map, current_owns_exit_lock,
                               locked_exit_row, locked_exit_col);
      break;
    }

    if (current_tick % vehicle->speed_ticks != 0) {
      continue;
    }

    int next_row, next_col;
    if (!vehicle_choose_next_position(vehicle, map, current_tick,
                                      &current_direction, &last_valid_direction,
                                      &next_row, &next_col)) {
      continue;
    }

    vehicle_handle_exit_lock_on_reroute(map, next_row, next_col,
                                        &current_owns_exit_lock,
                                        &locked_exit_row, &locked_exit_col);

    if (is_within_map_bounds(map, next_row, next_col)) {
      int exit_row = -1, exit_col = -1;

      if (!vehicle_try_reserve_movement(
              vehicle, map, next_row, next_col, current_direction, current_tick,
              current_owns_exit_lock, locked_exit_row, locked_exit_col,
              &exit_row, &exit_col)) {
        continue;
      }

      bool target_is_intersection =
          (map->cell_grid[next_row][next_col].type == INTERSECTION);

      vehicle_perform_move(vehicle, map, next_row, next_col,
                           target_is_intersection, exit_row, exit_col,
                           &current_owns_exit_lock, &locked_exit_row,
                           &locked_exit_col);

    } else {
      vehicle_exit_map_cleanup(vehicle, map, current_owns_exit_lock,
                               locked_exit_row, locked_exit_col);
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

  pthread_mutex_lock(&map->cell_grid[start_row][start_col].mutex);
  if (map->cell_grid[start_row][start_col].current_vehicle != NULL) {
    pthread_mutex_unlock(&map->cell_grid[start_row][start_col].mutex);
    free(vehicle);
    return NULL;
  }
  pthread_mutex_unlock(&map->cell_grid[start_row][start_col].mutex);

  if (pthread_create(&vehicle->thread_id, NULL, vehicle_lifecycle,
                     (void *)vehicle) != 0) {
    free(vehicle);
    return NULL;
  }

  return vehicle;
}

void vehicle_destroy(Vehicle *vehicle) {
  if (vehicle != NULL) {
    pthread_join(vehicle->thread_id, NULL);
    free(vehicle);
  }
}
