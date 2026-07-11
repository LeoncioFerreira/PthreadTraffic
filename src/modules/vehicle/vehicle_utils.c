#include "vehicle_utils.h"
#include "../deadlock/deadlock.h"
#include "../navigation/navigation.h"
#include "../traffic/traffic.h"
#include <pthread.h>
#include <stddef.h>

/**
 * Descrição: Utilitários de controle de movimentação de veículos, responsáveis
 * por toda a lógica mecânica de bloqueio, liberação e transição atômica entre
 * células do mapa, além do roteamento, simplificando o fluxo principal do
 * veículo. Autores: Leôncio Ferreira e André Wesley.
 */

void vehicle_exit_map_cleanup(Vehicle *vehicle, Map *map,
                              bool owns_current_lock, bool owns_exit_lock,
                              int locked_exit_row, int locked_exit_col) {

  if (owns_current_lock) {

    pthread_mutex_lock(&map_state_mutex);
    map->cell_grid[vehicle->row][vehicle->col].current_vehicle = NULL;
    pthread_mutex_unlock(&map_state_mutex);

    if (map->cell_grid[vehicle->row][vehicle->col].type == INTERSECTION) {
      traffic_release_capacity(vehicle->row, vehicle->col);
    }

    pthread_mutex_unlock(&map->cell_grid[vehicle->row][vehicle->col].mutex);
  }

  if (owns_exit_lock &&
      is_within_map_bounds(map, locked_exit_row, locked_exit_col)) {
    pthread_mutex_unlock(
        &map->cell_grid[locked_exit_row][locked_exit_col].mutex);
  }
}

bool vehicle_try_reserve_movement(Vehicle *vehicle, Map *map, int next_row,
                                  int next_col, char direction, uint64_t tick,
                                  bool owns_exit_lock, int locked_exit_row,
                                  int locked_exit_col, int *exit_row,
                                  int *exit_col) {
  bool current_is_intersection =
      (map->cell_grid[vehicle->row][vehicle->col].type == INTERSECTION);
  bool target_is_intersection =
      (map->cell_grid[next_row][next_col].type == INTERSECTION);

  if (target_is_intersection) {
    bool already_owns_target_lock =
        (owns_exit_lock && next_row == locked_exit_row &&
         next_col == locked_exit_col);

    if (!already_owns_target_lock) {
      if (!current_is_intersection &&
          !traffic_is_safe_to_enter(next_row, next_col, direction, tick)) {
        return false;
      }

      if (pthread_mutex_trylock(&map->cell_grid[next_row][next_col].mutex) !=
          0) {
        return false;
      }

      pthread_mutex_lock(&map_state_mutex);
      bool target_has_vehicle =
          (map->cell_grid[next_row][next_col].current_vehicle != NULL);
      pthread_mutex_unlock(&map_state_mutex);

      if (target_has_vehicle) {
        pthread_mutex_unlock(&map->cell_grid[next_row][next_col].mutex);
        return false;
      }
    }

    if (!deadlock_try_reserve_exit_cell(map, next_row, next_col, direction,
                                        exit_row, exit_col, tick)) {
      if (!already_owns_target_lock) {
        pthread_mutex_unlock(&map->cell_grid[next_row][next_col].mutex);
      }
      return false;
    }

    if (!already_owns_target_lock && !current_is_intersection &&
        !traffic_try_enter_capacity(next_row, next_col)) {
      pthread_mutex_unlock(&map->cell_grid[next_row][next_col].mutex);

      if (is_within_map_bounds(map, *exit_row, *exit_col)) {
        pthread_mutex_unlock(&map->cell_grid[*exit_row][*exit_col].mutex);
      }

      return false;
    }
  } else {
    bool already_owns_target_lock =
        (owns_exit_lock && next_row == locked_exit_row &&
         next_col == locked_exit_col);

    if (!already_owns_target_lock) {
      if (pthread_mutex_trylock(&map->cell_grid[next_row][next_col].mutex) !=
          0) {
        return false;
      }

      pthread_mutex_lock(&map_state_mutex);
      bool target_has_vehicle =
          (map->cell_grid[next_row][next_col].current_vehicle != NULL);
      pthread_mutex_unlock(&map_state_mutex);

      if (target_has_vehicle) {
        pthread_mutex_unlock(&map->cell_grid[next_row][next_col].mutex);
        return false;
      }
    }
  }

  return true;
}

void vehicle_perform_move(Vehicle *vehicle, Map *map, int next_row,
                          int next_col, bool target_is_intersection,
                          int exit_row, int exit_col, bool *owns_exit_lock,
                          int *locked_exit_row, int *locked_exit_col) {
  int previous_row = vehicle->row;
  int previous_col = vehicle->col;

  pthread_mutex_lock(&map_state_mutex);

  map->cell_grid[next_row][next_col].current_vehicle = vehicle;
  map->cell_grid[previous_row][previous_col].current_vehicle = NULL;

  vehicle->row = next_row;
  vehicle->col = next_col;

  pthread_mutex_unlock(&map_state_mutex);

  if (map->cell_grid[previous_row][previous_col].type == INTERSECTION) {
    bool previous_was_intersection =
        (map->cell_grid[previous_row][previous_col].type == INTERSECTION);
    bool current_is_intersection =
        (map->cell_grid[next_row][next_col].type == INTERSECTION);

    // Só libera a capacidade do semáforo se o carro saiu de vez do cruzamento!
    if (previous_was_intersection && !current_is_intersection) {
      traffic_release_capacity(previous_row, previous_col);
    }
  }

  pthread_mutex_unlock(&map->cell_grid[previous_row][previous_col].mutex);

  if (target_is_intersection) {
    *owns_exit_lock = true;
    *locked_exit_row = exit_row;
    *locked_exit_col = exit_col;
  } else {
    *owns_exit_lock = false;
    *locked_exit_row = -1;
    *locked_exit_col = -1;
  }
}

bool vehicle_choose_next_position(const Vehicle *vehicle, const Map *map,
                                  uint64_t tick, char *current_direction,
                                  char *last_valid_direction, int *next_row,
                                  int *next_col) {

  NavigationState nav_state = {.row = vehicle->row,
                               .col = vehicle->col,
                               .current_dir = *current_direction,
                               .last_valid_dir = *last_valid_direction,
                               .vehicle_id = vehicle->id};

  *current_direction = navigation_choose_direction(map, &nav_state, tick);
  *last_valid_direction = *current_direction;

  calculate_next_position(*current_direction, vehicle->row, vehicle->col,
                          next_row, next_col);

  return (*next_row != vehicle->row || *next_col != vehicle->col);
}

void vehicle_handle_exit_lock_on_reroute(Map *map, int next_row, int next_col,
                                         bool *owns_exit_lock,
                                         int *locked_exit_row,
                                         int *locked_exit_col) {
  if (*owns_exit_lock &&
      (next_row != *locked_exit_row || next_col != *locked_exit_col)) {
    pthread_mutex_unlock(
        &map->cell_grid[*locked_exit_row][*locked_exit_col].mutex);
    *owns_exit_lock = false;
    *locked_exit_row = -1;
    *locked_exit_col = -1;
  }
}