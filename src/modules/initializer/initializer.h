#ifndef INITIALIZER_H
#define INITIALIZER_H

#include "../map/map.h"
#include <stdbool.h>

typedef struct {
  int max_vehicles;
  int tick_ms;
  char *map_path;
} Config;

bool system_init(int argc, char *argv[], Config *cfg, Map **mapa);
void system_wait_for_shutdown(void);
void system_cleanup(void);

#endif
