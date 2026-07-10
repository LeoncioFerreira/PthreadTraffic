/**
 * Descrição: Protótipos para geração dinâmica e temporizada de novos veículos.
 * Autor: Salomão Rodrigues
 */

#ifndef SPAWNER_H
#define SPAWNER_H

#include "../map/map.h"
#include <stdbool.h>

bool spawner_start(int max_vehicles, Map *mapa);
void spawner_stop(void);
void spawner_cleanup(void);

#endif
