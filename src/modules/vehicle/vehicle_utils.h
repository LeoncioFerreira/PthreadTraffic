/**
 * Descrição: Funções utilitárias e regras auxiliares para movimentação de
 * veículos. Autor: Leôncio Ferreira
 */

#ifndef VEHICLE_UTILS_H
#define VEHICLE_UTILS_H

#include "vehicle.h"
#include <stdbool.h>
#include <stdint.h>

void vehicle_exit_map_cleanup(Vehicle *vehicle, Map *map,
                              bool owns_current_lock, bool owns_exit_lock,
                              int locked_exit_row, int locked_exit_col);

bool vehicle_try_reserve_movement(Vehicle *vehicle, Map *map, int next_row,
                                  int next_col, char direction, uint64_t tick,
                                  bool owns_exit_lock, int locked_exit_row,
                                  int locked_exit_col, int *exit_row,
                                  int *exit_col);

void vehicle_perform_move(Vehicle *vehicle, Map *map, int next_row,
                          int next_col, bool target_is_intersection,
                          int exit_row, int exit_col, bool *owns_exit_lock,
                          int *locked_exit_row, int *locked_exit_col);

bool vehicle_choose_next_position(const Vehicle *vehicle, const Map *map,
                                  uint64_t tick, char *current_direction,
                                  char *last_valid_direction, int *next_row,
                                  int *next_col);

void vehicle_handle_exit_lock_on_reroute(Map *map, int next_row, int next_col,
                                         bool *owns_exit_lock,
                                         int *locked_exit_row,
                                         int *locked_exit_col);

#endif
