#ifndef VEHICLE_H
#define VEHICLE_H

#include "../map/map.h"
#include <pthread.h>

/**
 * Descrição: Definição da estrutura de dados e das funções de ciclo de vidados
 * veículos. O veículo navega pelas células utilizando o mapa e sincroniza o seu
 * movimento baseado nos "ticks" globais emitidos pelo módulo de relógio.
 * Autor: Leôncio Ferreira
 */

typedef enum {
  FAST_CAR,   // Move a cada 1 tick
  MEDIUM_CAR, // Move a cada 2 ticks
  SLOW_CAR    // Move a cada 4 ticks
} VehicleType;

typedef struct Vehicle {
  int id;
  int row;
  int col;
  int speed_ticks;
  VehicleType type;
  Map *map_ref;
  pthread_t thread_id;
} Vehicle;

void *vehicle_lifecycle(void *args);
Vehicle *vehicle_create_and_start(int id, int start_row, int start_col,
                                  int speed_ticks, VehicleType type, Map *map);
void vehicle_destroy(Vehicle *v);
#endif
