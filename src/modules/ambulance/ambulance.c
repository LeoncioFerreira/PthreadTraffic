#include "ambulance.h"
#include "../traffic/traffic.h"

/**
 * Descrição: Implementação da lógica da ambulância (veículo com prioridade
 * máxima), capaz de solicitar a abertura dos semáforos e liberar cruzamentos.
 * Autor: Leôncio Ferreira
 */

void ambulance_request_path(Vehicle *v, Map *map, int target_row,
                            int target_col, char direction) {
  if (v == NULL || map == NULL || v->type != AMBULANCE)
    return;

  if (map->cell_grid[target_row][target_col].type == INTERSECTION) {
    for (int i = target_row - 1; i <= target_row + 1; i++) {
      for (int j = target_col - 1; j <= target_col + 1; j++) {
        if (is_within_map_bounds(map, i, j) &&
            map->cell_grid[i][j].type == INTERSECTION) {
          traffic_request_priority(i, j, direction);
        }
      }
    }
  }
}

void ambulance_clear_path(Vehicle *v, Map *map, int prev_row, int prev_col) {
  if (v == NULL || map == NULL || v->type != AMBULANCE)
    return;

  if (map->cell_grid[prev_row][prev_col].type == INTERSECTION) {
    bool current_is_intersection = false;
    if (is_within_map_bounds(map, v->row, v->col)) {
      current_is_intersection =
          (map->cell_grid[v->row][v->col].type == INTERSECTION);
    }

    if (!current_is_intersection) {
      for (int i = prev_row - 1; i <= prev_row + 1; i++) {
        for (int j = prev_col - 1; j <= prev_col + 1; j++) {
          if (is_within_map_bounds(map, i, j) &&
              map->cell_grid[i][j].type == INTERSECTION) {
            traffic_release_priority(i, j);
          }
        }
      }
    }
  }
}
