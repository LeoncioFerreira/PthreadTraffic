#include "vehicle.h"
#include "../ambulance/ambulance.h"
#include "../clock/clock.h"
#include "../logger/logger.h"
#include "../navigation/navigation.h"
#include "../traffic/traffic.h"
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

  pthread_mutex_lock(&map->cell_grid[vehicle->row][vehicle->col].mutex);
  pthread_mutex_lock(&map_state_mutex);
  map->cell_grid[vehicle->row][vehicle->col].current_vehicle = vehicle;
  pthread_mutex_unlock(&map_state_mutex);

  logger_write(LOG_INFO, "Veículo %d (%s) entrou no mapa na posição [%d][%d]",
               vehicle->id, vehicle->type == AMBULANCE ? "Ambulância" : "Civil",
               vehicle->row, vehicle->col);

  bool current_owns_exit_lock = false;
  int locked_exit_row = -1;
  int locked_exit_col = -1;

  char last_valid_direction =
      map->cell_grid[vehicle->row][vehicle->col].direction;
  if (last_valid_direction == ' ') {
    last_valid_direction = 'L';
  }

  char current_direction = last_valid_direction;
  bool exited_normally = false;

  while (clock_is_running()) {
    uint64_t current_tick = clock_wait_for_tick();

    if (!clock_is_running()) {
      break;
    }

    if (current_tick % vehicle->speed_ticks != 0) {
      continue;
    }

    int next_row, next_col;
    char previous_direction = current_direction;
    char previous_last_valid_direction = last_valid_direction;
    if (!vehicle_choose_next_position(vehicle, map, current_tick,
                                      &current_direction, &last_valid_direction,
                                      &next_row, &next_col)) {
      current_direction = previous_direction;
      last_valid_direction = previous_last_valid_direction;
      continue;
    }

    vehicle_handle_exit_lock_on_reroute(map, next_row, next_col,
                                        &current_owns_exit_lock,
                                        &locked_exit_row, &locked_exit_col);

    if (is_within_map_bounds(map, next_row, next_col)) {
      int exit_row = -1, exit_col = -1;

      bool target_is_intersection =
          (map->cell_grid[next_row][next_col].type == INTERSECTION);

      bool current_is_intersection =
          (map->cell_grid[vehicle->row][vehicle->col].type == INTERSECTION);

      // Ambulância pede passagem apenas quando está entrando no cruzamento,
      // não quando já está se movendo dentro dele.
      if (vehicle->type == AMBULANCE && target_is_intersection &&
          !current_is_intersection) {
        ambulance_request_path(vehicle, map, next_row, next_col,
                               current_direction);
      }

      if (target_is_intersection && !current_is_intersection &&
          !traffic_is_safe_to_enter(next_row, next_col, current_direction,
                                    current_tick)) {

        traffic_wait_for_green(next_row, next_col, current_direction);

        if (!clock_is_running()) {
          break;
        }

        pthread_mutex_lock(&clock_mutex);
        current_tick = global_tick;
        pthread_mutex_unlock(&clock_mutex);
      }

      if (!vehicle_try_reserve_movement(
              vehicle, map, next_row, next_col, current_direction, current_tick,
              current_owns_exit_lock, locked_exit_row, locked_exit_col,
              &exit_row, &exit_col)) {
        current_direction = previous_direction;
        last_valid_direction = previous_last_valid_direction;
        continue;
      }

      int old_row = vehicle->row;
      int old_col = vehicle->col;

      vehicle_perform_move(vehicle, map, next_row, next_col,
                           target_is_intersection, exit_row, exit_col,
                           &current_owns_exit_lock, &locked_exit_row,
                           &locked_exit_col);

      logger_write(LOG_INFO, "Veículo %d moveu-se de [%d][%d] para [%d][%d]",
                   vehicle->id, old_row, old_col, next_row, next_col);

      ambulance_clear_path(vehicle, map, old_row, old_col);
    } else {
      logger_write(LOG_INFO, "Veículo %d chegou ao fim da via e saiu do mapa",
                   vehicle->id);
      exited_normally = true;
      break;
    }
  }

  if (!exited_normally) {
    logger_write(LOG_INFO,
                 "Veículo %d saindo do mapa devido ao encerramento do sistema",
                 vehicle->id);
  }

  vehicle_exit_map_cleanup(vehicle, map, true, current_owns_exit_lock,
                           locked_exit_row, locked_exit_col);

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

  pthread_mutex_lock(&map_state_mutex);
  map->cell_grid[start_row][start_col].current_vehicle = vehicle;
  pthread_mutex_unlock(&map_state_mutex);
  pthread_mutex_unlock(&map->cell_grid[start_row][start_col].mutex);

  if (pthread_create(&vehicle->thread_id, NULL, vehicle_lifecycle,
                     (void *)vehicle) != 0) {
    pthread_mutex_lock(&map->cell_grid[start_row][start_col].mutex);
    pthread_mutex_lock(&map_state_mutex);
    map->cell_grid[start_row][start_col].current_vehicle = NULL;
    pthread_mutex_unlock(&map_state_mutex);
    pthread_mutex_unlock(&map->cell_grid[start_row][start_col].mutex);
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