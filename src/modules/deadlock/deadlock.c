#include "deadlock.h"
#include "../navigation/navigation.h"
#include "../traffic/traffic.h"
#include <pthread.h>

/**
 * Descrição: Implementação da estratégia anti-deadlock por meio de Look-Ahead
 * estendido. Contém rotinas para cálculo de posições futuras e a reserva das
 * células de saída dos cruzamentos. Autor da estratégia: André Wesley
 */

bool deadlock_try_reserve_exit_cell(const Map *map, int inter_row,
                                    int inter_col, char direction,
                                    int *exit_row, int *exit_col,
                                    uint64_t tick) {
  calculate_next_position(direction, inter_row, inter_col, exit_row, exit_col);

  if (!is_within_map_bounds(map, *exit_row, *exit_col)) {
    return true;
  }

  if (map->cell_grid[*exit_row][*exit_col].type == INTERSECTION) {
    if (!traffic_is_safe_to_enter(*exit_row, *exit_col, direction, tick)) {
      return false;
    }
  }

  int result =
      pthread_mutex_trylock(&map->cell_grid[*exit_row][*exit_col].mutex);
  return (result == 0);
}
