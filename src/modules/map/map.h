#ifndef MAP_H
#define MAP_H

#include <pthread.h>
/**
 * Descrição: Define as principais estruturas de dados necessárias para o
 * funcionamento do mapa, como o tipo de terreno (CellType), a célula (Cell -
 * que armazena a direção e garante a exclusão mútua através do Mutex) e a
 * matriz principal do Mapa (Map). Autor: Paulo Gabriel
 */

typedef enum celltype {
  EMPTY,
  ROADS,
  INTERSECTION,
} CellType;

typedef struct cell {
  CellType type;  // empty, road or intersection
  char direction; // 'N' (Norte), 'S' (Sul), 'L' (Leste), 'O' (Oeste) ou ' '
                  // (vazio)
  pthread_mutex_t mutex;

} Cell;

typedef struct map {
  int rows; // lines
  int columns;
  Cell **cell_grid;
} Map;

Map *load_map(const char *path_file);
void destroy(Map *mapa);

#endif