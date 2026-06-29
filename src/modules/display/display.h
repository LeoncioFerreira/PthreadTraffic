#ifndef DISPLAY_H
#define DISPLAY_H

#include "../map/map.h"

// Inicializa a thread de visualização
void display_start(Map *map);

// Para a thread de visualização
void display_stop(void);

#endif