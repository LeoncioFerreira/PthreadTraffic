#ifndef DEADLOCK_H
#define DEADLOCK_H

#include "../map/map.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * Descrição: Definição das funções para a estratégia de prevenção de deadlocks.
 * O módulo implementa a estratégia de Look-Ahead estendido e reserva de
 * recursos "All-or-Nothing" para garantir que os veículos atravessem os
 * cruzamentos com segurança. Autor da estratégia: André Wesley
 */

/*
 * Tenta reservar a célula de saída após o cruzamento correspondente à direção
 * do veículo. Retorna true se a reserva for bem-sucedida, false caso contrário.
 */
bool deadlock_try_reserve_exit_cell(const Map *map, int inter_row,
                                    int inter_col, char direction,
                                    int *exit_row, int *exit_col,
                                    uint64_t tick);

#endif
