#include "vehicle.h"
#include "../clock/clock.h"
#include "../traffic/traffic.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * Descrição: Implementação do ciclo de vida das threads de veículos, das
 * lógicas de exclusão mútua na simulação e da estratégia anti-deadlock por meio
 * de Look-Ahead estendido.
 * Autores: Leôncio Ferreira e André Wesley.
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

extern volatile bool keep_running;

static bool try_reserve_exit_cell(const Map *map, int inter_row, int inter_col,
                                  char direction, int *exit_row,
                                  int *exit_col) {
  calculate_next_position(direction, inter_row, inter_col, exit_row, exit_col);

  if (!is_within_map_bounds(map, *exit_row, *exit_col)) {
    return true;
  }

  if (map->cell_grid[*exit_row][*exit_col].type == INTERSECTION) {
    if (!traffic_is_green(*exit_row, *exit_col, direction)) {
      return false;
    }
  }

  int result =
      pthread_mutex_trylock(&map->cell_grid[*exit_row][*exit_col].mutex);
  return (result == 0);
}

void *vehicle_lifecycle(void *arg) {
  Vehicle *vehicle = (Vehicle *)arg;
  Map *map = vehicle->map_ref;

  // 1. Trava a célula inicial e se registra nela
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

  while (keep_running) {
    uint64_t current_tick = wait_for_next_tick();

    if (!clock_is_running()) {
      map->cell_grid[vehicle->row][vehicle->col].current_vehicle = NULL;
      pthread_mutex_unlock(&map->cell_grid[vehicle->row][vehicle->col].mutex);
      if (current_owns_exit_lock &&
          is_within_map_bounds(map, locked_exit_row, locked_exit_col)) {
        pthread_mutex_unlock(
            &map->cell_grid[locked_exit_row][locked_exit_col].mutex);
      }
      break;
    }

    if (current_tick % vehicle->speed_ticks != 0) {
      continue;
    }

    // ======== INÍCIO DA LÓGICA DE DIREÇÃO INTELIGENTE ========
    char cell_dir = map->cell_grid[vehicle->row][vehicle->col].direction;
    CellType type = map->cell_grid[vehicle->row][vehicle->col].type;

    if (cell_dir != ' ') {
      // Pista normal: obedece a seta do chão
      current_direction = cell_dir;
      last_valid_direction = current_direction;
    } else if (type == INTERSECTION) {
      char valid_turns[4];
      int num_turns = 0;

      // 1. Prioriza seguir reto
      valid_turns[num_turns++] = current_direction;
      valid_turns[num_turns++] = current_direction;

      // 2. O "Radar": Olha até 2 blocos à frente
      if (current_direction != 'S' && current_direction != 'v') {
        for (int i = 1; i <= 2; i++) {
          if (is_within_map_bounds(map, vehicle->row - i, vehicle->col)) {
            char d = map->cell_grid[vehicle->row - i][vehicle->col].direction;
            if (d == 'N' || d == '^') {
              valid_turns[num_turns++] = '^';
              break;
            }
            if (d != ' ' && d != '+') {
              break;
            }
          }
        }
      }
      if (current_direction != 'N' && current_direction != '^') {
        for (int i = 1; i <= 2; i++) {
          if (is_within_map_bounds(map, vehicle->row + i, vehicle->col)) {
            char d = map->cell_grid[vehicle->row + i][vehicle->col].direction;
            if (d == 'S' || d == 'v') {
              valid_turns[num_turns++] = 'v';
              break;
            }
            if (d != ' ' && d != '+') {
              break;
            }
          }
        }
      }
      if (current_direction != 'O' && current_direction != '<') {
        for (int i = 1; i <= 2; i++) {
          if (is_within_map_bounds(map, vehicle->row, vehicle->col + i)) {
            char d = map->cell_grid[vehicle->row][vehicle->col + i].direction;
            if (d == 'L' || d == '>') {
              valid_turns[num_turns++] = '>';
              break;
            }
            if (d != ' ' && d != '+') {
              break;
            }
          }
        }
      }
      if (current_direction != 'L' && current_direction != '>') {
        for (int i = 1; i <= 2; i++) {
          if (is_within_map_bounds(map, vehicle->row, vehicle->col - i)) {
            char d = map->cell_grid[vehicle->row][vehicle->col - i].direction;
            if (d == 'O' || d == '<') {
              valid_turns[num_turns++] = '<';
              break;
            }
            if (d != ' ' && d != '+') {
              break;
            }
          }
        }
      }

      // 3. Sorteia uma das opções válidas
      int random_choice = (rand() + vehicle->id + current_tick) % num_turns;
      current_direction = valid_turns[random_choice];
      last_valid_direction = current_direction;
    } else {
      current_direction = last_valid_direction;
    }

    int next_row, next_col;
    calculate_next_position(current_direction, vehicle->row, vehicle->col,
                            &next_row, &next_col);

    // PROTEÇÃO ANTI-DEADLOCK: Se o carro não sabe para onde ir
    if (next_row == vehicle->row && next_col == vehicle->col) {
      continue;
    }

    if (is_within_map_bounds(map, next_row, next_col)) {
      int exit_row = -1, exit_col = -1;
      bool target_is_intersection =
          (map->cell_grid[next_row][next_col].type == INTERSECTION);

      if (target_is_intersection) {
        bool already_owns_target_lock =
            (current_owns_exit_lock && next_row == locked_exit_row &&
             next_col == locked_exit_col);

        if (!already_owns_target_lock) {
          if (!traffic_is_green(next_row, next_col, current_direction)) {
            continue;
          }

          if (pthread_mutex_trylock(
                  &map->cell_grid[next_row][next_col].mutex) != 0) {
            continue;
          }
        }

        if (!traffic_is_green(next_row, next_col, current_direction) ||
            !try_reserve_exit_cell(map, next_row, next_col, current_direction,
                                   &exit_row, &exit_col)) {

          if (!already_owns_target_lock) {
            pthread_mutex_unlock(&map->cell_grid[next_row][next_col].mutex);
          }
          continue;
        }
      } else {
        if (current_owns_exit_lock && next_row == locked_exit_row &&
            next_col == locked_exit_col) {
        } else {
          if (pthread_mutex_trylock(
                  &map->cell_grid[next_row][next_col].mutex) != 0) {
            continue;
          }
        }
      }

      // Atualiza os ponteiros do mapa com Lock Coupling
      map->cell_grid[next_row][next_col].current_vehicle = vehicle;
      map->cell_grid[vehicle->row][vehicle->col].current_vehicle = NULL;

      int previous_row = vehicle->row;
      int previous_col = vehicle->col;

      vehicle->row = next_row;
      vehicle->col = next_col;

      pthread_mutex_unlock(&map->cell_grid[previous_row][previous_col].mutex);

      if (target_is_intersection) {
        current_owns_exit_lock = true;
        locked_exit_row = exit_row;
        locked_exit_col = exit_col;
      } else {
        current_owns_exit_lock = false;
        locked_exit_row = -1;
        locked_exit_col = -1;
      }

    } else {
      // O carro saiu do mapa
      map->cell_grid[vehicle->row][vehicle->col].current_vehicle = NULL;
      pthread_mutex_unlock(&map->cell_grid[vehicle->row][vehicle->col].mutex);
      if (current_owns_exit_lock &&
          is_within_map_bounds(map, locked_exit_row, locked_exit_col)) {
        pthread_mutex_unlock(
            &map->cell_grid[locked_exit_row][locked_exit_col].mutex);
      }
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
