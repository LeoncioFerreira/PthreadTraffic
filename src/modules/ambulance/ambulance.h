#ifndef AMBULANCE_H
#define AMBULANCE_H

#include "../map/map.h"
#include "../vehicle/vehicle.h"

/**
 * Descrição: Responsável por gerenciar a lógica de prioridade de veículos de
 * emergência. Autor: Leôncio Ferreira
 */

void ambulance_request_path(Vehicle *v, Map *map, int target_row,
                            int target_col, char direction);

void ambulance_clear_path(Vehicle *v, Map *map, int prev_row, int prev_col);

#endif
