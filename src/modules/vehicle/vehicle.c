#include "vehicle.h"
#include "../clock/clock.h"
#include "../traffic/traffic.h"
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

extern volatile bool keep_running;

void *vehicle_lifecycle(void *arg) {
  Vehicle *vehicle = (Vehicle *)arg;
  Map *map = vehicle->map_ref;

  // 1. Trava a célula inicial e se registra nela
  pthread_mutex_lock(&map->cell_grid[vehicle->row][vehicle->col].mutex);
  map->cell_grid[vehicle->row][vehicle->col].current_vehicle = vehicle;

  // Guarda a direção inicial
  char current_direction = map->cell_grid[vehicle->row][vehicle->col].direction;

  while (keep_running) {
    uint64_t current_tick = wait_for_next_tick();

    if (current_tick % vehicle->speed_ticks != 0) {
      continue;
    }

    // ======== INÍCIO DA LÓGICA DE DIREÇÃO INTELIGENTE ========
    char cell_dir = map->cell_grid[vehicle->row][vehicle->col].direction;
    CellType type = map->cell_grid[vehicle->row][vehicle->col].type;

    if (cell_dir != ' ') {
      // Pista normal: obedece a seta do chão
      current_direction = cell_dir;

    } else if (type == INTERSECTION) {
      char valid_turns[4];
      int num_turns = 0;

      // 1. Prioriza seguir reto (adicionamos duas vezes para dar peso na
      // roleta)
      valid_turns[num_turns++] = current_direction;
      valid_turns[num_turns++] = current_direction;

      // 2. O "Radar": Olha até 2 blocos à frente para achar a faixa correta

      // Testa virar para o Norte (Cima)
      if (current_direction != 'S' && current_direction != 'v') {
        for (int i = 1; i <= 2; i++) {
          if (is_within_map_bounds(map, vehicle->row - i, vehicle->col)) {
            char d = map->cell_grid[vehicle->row - i][vehicle->col].direction;
            if (d == 'N' || d == '^') {
              valid_turns[num_turns++] = '^';
              break;
            }
            if (d != ' ' && d != '+')
              break; // Bateu numa parede ou contramão, aborta!
          }
        }
      }
      // Testa virar para o Sul (Baixo)
      if (current_direction != 'N' && current_direction != '^') {
        for (int i = 1; i <= 2; i++) {
          if (is_within_map_bounds(map, vehicle->row + i, vehicle->col)) {
            char d = map->cell_grid[vehicle->row + i][vehicle->col].direction;
            if (d == 'S' || d == 'v') {
              valid_turns[num_turns++] = 'v';
              break;
            }
            if (d != ' ' && d != '+')
              break;
          }
        }
      }
      // Testa virar para o Leste (Direita)
      if (current_direction != 'O' && current_direction != '<') {
        for (int i = 1; i <= 2; i++) {
          if (is_within_map_bounds(map, vehicle->row, vehicle->col + i)) {
            char d = map->cell_grid[vehicle->row][vehicle->col + i].direction;
            if (d == 'L' || d == '>') {
              valid_turns[num_turns++] = '>';
              break;
            }
            if (d != ' ' && d != '+')
              break;
          }
        }
      }
      // Testa virar para o Oeste (Esquerda)
      if (current_direction != 'L' && current_direction != '>') {
        for (int i = 1; i <= 2; i++) {
          if (is_within_map_bounds(map, vehicle->row, vehicle->col - i)) {
            char d = map->cell_grid[vehicle->row][vehicle->col - i].direction;
            if (d == 'O' || d == '<') {
              valid_turns[num_turns++] = '<';
              break;
            }
            if (d != ' ' && d != '+')
              break;
          }
        }
      }

      // 3. Sorteia uma das opções válidas
      int random_choice = (rand() + vehicle->id + current_tick) % num_turns;
      current_direction = valid_turns[random_choice];
    }

    int next_row, next_col;
    calculate_next_position(current_direction, vehicle->row, vehicle->col,
                            &next_row, &next_col);

    // PROTEÇÃO ANTI-DEADLOCK: Se o carro não sabe para onde ir, ele não tenta
    // trancar a própria célula novamente.
    if (next_row == vehicle->row && next_col == vehicle->col) {
      continue;
    }

    if (is_within_map_bounds(map, next_row, next_col)) {

      if (map->cell_grid[next_row][next_col].type == INTERSECTION) {
        bool safely_entered = false;

        while (!safely_entered) {
          traffic_wait_for_green(next_row, next_col, current_direction);

          pthread_mutex_lock(&map->cell_grid[next_row][next_col].mutex);

          if (traffic_is_green(next_row, next_col, current_direction)) {
            safely_entered = true;
          } else {
            pthread_mutex_unlock(&map->cell_grid[next_row][next_col].mutex);
          }
        }
      } else {
        pthread_mutex_lock(&map->cell_grid[next_row][next_col].mutex);
      }

      // Atualiza os ponteiros do mapa com Lock Coupling
      map->cell_grid[next_row][next_col].current_vehicle = vehicle;
      map->cell_grid[vehicle->row][vehicle->col].current_vehicle = NULL;

      pthread_mutex_unlock(&map->cell_grid[vehicle->row][vehicle->col].mutex);

      vehicle->row = next_row;
      vehicle->col = next_col;

    } else {
      // O carro saiu do mapa
      map->cell_grid[vehicle->row][vehicle->col].current_vehicle = NULL;
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
  // Força a quebra do Deadlock abortando a thread antes de dar o join
  pthread_cancel(vehicle->thread_id);
  pthread_join(vehicle->thread_id, NULL);
  free(vehicle);
}