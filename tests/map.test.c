#include "../src/modules/map/map.h"
#include "../src/modules/map/map.c"
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>

void setUp() {
  FILE *f = fopen("mini_mapa.txt", "w");
  if (f) {
    fprintf(f, ">v\n");
    fprintf(f, "+ \n");
    fclose(f);
  }
}

void tearDown() { remove("mini_mapa.txt"); }

void test_carregar_mapa_arquivo_inexistente() {
  Map *mapa = load_map("arquivo_fantasma.txt");

  TEST_ASSERT_NULL(mapa);
}

void test_carregar_mapa_dimensoes_e_celulas() {
  Map *mapa = load_map("mini_mapa.txt");

  TEST_ASSERT_NOT_NULL(mapa);

  TEST_ASSERT_EQUAL_INT(2, mapa->rows);
  TEST_ASSERT_EQUAL_INT(2, mapa->columns);

  TEST_ASSERT_EQUAL_INT(ROADS, mapa->cell_grid[0][0].type);
  TEST_ASSERT_EQUAL_CHAR('L', mapa->cell_grid[0][0].direction);

  TEST_ASSERT_EQUAL_INT(INTERSECTION, mapa->cell_grid[1][0].type);

  TEST_ASSERT_EQUAL_INT(EMPTY, mapa->cell_grid[1][1].type);

  destroy(mapa);
}

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_carregar_mapa_arquivo_inexistente);
  RUN_TEST(test_carregar_mapa_dimensoes_e_celulas);

  return UNITY_END();
}