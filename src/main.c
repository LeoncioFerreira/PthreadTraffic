#include "modules/map/map.h"
#include <stdio.h>
#include <stdlib.h>

int main() {

  Map *mapa = load_map("mapa.txt");
  if (mapa == NULL) {
    printf("Erro ao carregar o mapa.\n");
    return EXIT_FAILURE;
  }

  printf("Simulador de Tráfego Pthread\n");
  printf("Mapa carregado com sucesso! Dimensões: %d linhas x %d colunas\n",
         mapa->rows, mapa->columns);

  destroy(mapa);
  return EXIT_SUCCESS;
}
