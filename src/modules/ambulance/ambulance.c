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
    traffic_request_priority(target_row, target_col, direction);
  }
}

void ambulance_clear_path(Vehicle *v, Map *map, int prev_row, int prev_col) {
  if (v == NULL || map == NULL || v->type != AMBULANCE)
    return;

  if (map->cell_grid[prev_row][prev_col].type == INTERSECTION) {
    traffic_release_priority(prev_row, prev_col);
  }
}
