/**
 * Descrição: Definições e funções para interface com o terminal ASCII.
 * Autor: Salomão Rodrigues
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include "../map/map.h"

void display_print_legend(void);

// Inicializa a thread de visualização
void display_start(Map *map);

// Para a thread de visualização
void display_stop(void);

#endif