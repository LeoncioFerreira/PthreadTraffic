/**
 * Descrição: Testes unitários para o módulo de semáforos, validando a
 * inicialização, a liberação de direções horizontais/verticais e o bloqueio.
 * Autor: Paulo
 */

#include "../src/modules/traffic/traffic.h"
#include "../src/modules/map/map.h"
#include "vendor/unity.h"
#include <stdbool.h>
#include <stdlib.h>

volatile bool keep_running = true;

static Map *test_map = NULL;

void setUp(void) {

  test_map = (Map *)malloc(sizeof(Map));
  if (test_map == NULL) {
    TEST_FAIL_MESSAGE("malloc falhou em setUp: sem memória para Map");
    return;
  }
  test_map->rows = 3;

  test_map->columns = 3;
  test_map->cell_grid = (Cell **)malloc(3 * sizeof(Cell *));

  for (int i = 0; i < 3; i++) {
    test_map->cell_grid[i] = (Cell *)malloc(3 * sizeof(Cell));
    for (int j = 0; j < 3; j++) {
      test_map->cell_grid[i][j].type = EMPTY;
      test_map->cell_grid[i][j].direction = ' ';
      pthread_mutex_init(&test_map->cell_grid[i][j].mutex, NULL);
    }
  }

  test_map->cell_grid[1][1].type = INTERSECTION;

  traffic_init(test_map, 5);
}

void tearDown(void) {
  traffic_stop();
  traffic_destroy();

  // Limpa a memória do mapa falso
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      pthread_mutex_destroy(&test_map->cell_grid[i][j].mutex);
    }
    free(test_map->cell_grid[i]);
  }
  free(test_map->cell_grid);
  free(test_map);
}

void test_traffic_starts_horizontal_green(void) {

  TEST_ASSERT_TRUE_MESSAGE(traffic_is_green(1, 1, 'L'),
                           "Carro para o Leste deveria ter sinal verde");
  TEST_ASSERT_TRUE_MESSAGE(traffic_is_green(1, 1, 'O'),
                           "Carro para o Oeste deveria ter sinal verde");
  TEST_ASSERT_TRUE_MESSAGE(traffic_is_green(1, 1, '>'),
                           "Símbolo '>' deveria ser reconhecido como verde");
  TEST_ASSERT_TRUE_MESSAGE(traffic_is_green(1, 1, '<'),
                           "Símbolo '<' deveria ser reconhecido como verde");
}

void test_traffic_starts_vertical_red(void) {

  TEST_ASSERT_FALSE_MESSAGE(traffic_is_green(1, 1, 'N'),
                            "Carro para o Norte deveria estar bloqueado");
  TEST_ASSERT_FALSE_MESSAGE(traffic_is_green(1, 1, 'S'),
                            "Carro para o Sul deveria estar bloqueado");
  TEST_ASSERT_FALSE_MESSAGE(traffic_is_green(1, 1, '^'),
                            "Símbolo '^' deveria estar bloqueado");
  TEST_ASSERT_FALSE_MESSAGE(traffic_is_green(1, 1, 'v'),
                            "Símbolo 'v' deveria estar bloqueado");
}

void test_traffic_ignores_non_intersection(void) {

  TEST_ASSERT_TRUE_MESSAGE(
      traffic_is_green(0, 0, 'N'),
      "Células sem cruzamento devem sempre retornar verde (true)");
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_traffic_starts_horizontal_green);
  RUN_TEST(test_traffic_starts_vertical_red);
  RUN_TEST(test_traffic_ignores_non_intersection);

  return UNITY_END();
}