#ifndef NAVIGATION_H
#define NAVIGATION_H

#include "../map/map.h"
#include <stdint.h>

/**
 * Descrição: Definição da estrutura de dados NavigationState e declaração da
 * função de decisão de navegação de veículos no mapa.
 * Autor: Salomão Rodrigues
 */

typedef struct {
  int row;
  int col;
  char current_dir;
  char last_valid_dir;
  int vehicle_id;
} NavigationState;

char navigation_choose_direction(const Map *map, const NavigationState *state,
                                 uint64_t tick);

void calculate_next_position(char direction, int current_row, int current_col,
                             int *next_row, int *next_col);

#endif
