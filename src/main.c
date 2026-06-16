#include "modules/map/map.h"
#include <stdio.h>

int main() {

  Map *meu_mapa = load_map("mapa.txt");
  if (meu_mapa != NULL) {
    printf("Mapa carregado com sucesso! Linhas: %d, Colunas: %d\n",
           meu_mapa->rows, meu_mapa->columns);

    destroy(meu_mapa);
    return 0;
  }
}
