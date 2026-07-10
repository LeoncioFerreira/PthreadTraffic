#include "navigation.h"
#include <stdlib.h>

/**
 * Descrição: Implementação da lógica de decisão de direção de veículos
 * (navegação inteligente) em interseções e pistas normais do mapa.
 * Autor: Salomão Rodrigues
 */

char navigation_choose_direction(const Map *map, const NavigationState *state,
                                 uint64_t tick) {
  char cell_dir = map->cell_grid[state->row][state->col].direction;
  CellType type = map->cell_grid[state->row][state->col].type;

  if (cell_dir != ' ') {
    return cell_dir;
  } else if (type == INTERSECTION) {
    char valid_turns[8];
    int num_turns = 0;

    // 1. Prioriza seguir em frente (adiciona duas vezes)
    valid_turns[num_turns++] = state->current_dir;
    valid_turns[num_turns++] = state->current_dir;

    // 2. O "Radar": Olha até 2 células à frente
    // Norte
    if (state->current_dir != 'S' && state->current_dir != 'v') {
      for (int i = 1; i <= 2; i++) {
        int r = state->row - i;
        if (r >= 0 && r < map->rows && state->col >= 0 &&
            state->col < map->columns) {
          char d = map->cell_grid[r][state->col].direction;
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
    // Sul
    if (state->current_dir != 'N' && state->current_dir != '^') {
      for (int i = 1; i <= 2; i++) {
        int r = state->row + i;
        if (r >= 0 && r < map->rows && state->col >= 0 &&
            state->col < map->columns) {
          char d = map->cell_grid[r][state->col].direction;
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
    // Leste
    if (state->current_dir != 'O' && state->current_dir != '<') {
      for (int i = 1; i <= 2; i++) {
        int c = state->col + i;
        if (state->row >= 0 && state->row < map->rows && c >= 0 &&
            c < map->columns) {
          char d = map->cell_grid[state->row][c].direction;
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
    // Oeste
    if (state->current_dir != 'L' && state->current_dir != '>') {
      for (int i = 1; i <= 2; i++) {
        int c = state->col - i;
        if (state->row >= 0 && state->row < map->rows && c >= 0 &&
            c < map->columns) {
          char d = map->cell_grid[state->row][c].direction;
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
    int random_choice = (rand() + state->vehicle_id + tick) % num_turns;
    return valid_turns[random_choice];
  }

  return state->last_valid_dir;
}

void calculate_next_position(char direction, int current_row, int current_col,
                             int *next_row, int *next_col) {
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
