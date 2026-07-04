/**
 * Descrição: Testes unitários para o módulo de semáforos, validando a
 * inicialização, a liberação de direções horizontais/verticais e o bloqueio.
 * Autor: Paulo
 */

#include "../src/modules/traffic/traffic.h"
#include "../src/modules/clock/clock.h"
#include "../src/modules/map/map.h"
#include "vendor/unity.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

volatile bool keep_running = true;

static Map *test_map = NULL;
static pthread_t wait_thread;
static volatile bool wait_finished = false;
static bool traffic_started = false;

static void *wait_for_vertical_green(void *arg) {
  (void)arg;
  traffic_wait_for_green(1, 1, 'N');
  wait_finished = true;
  return NULL;
}

void setUp(void) {
  clock_init();
  traffic_started = false;

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
  if (traffic_started) {
    traffic_stop();
  }
  if (clock_is_running()) {
    clock_stop();
  }
  traffic_destroy();
  clock_destroy();

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

void test_traffic_wait_for_green_blocks_until_signal_changes(void) {
  wait_finished = false;

  traffic_start();
  traffic_started = true;
  pthread_create(&wait_thread, NULL, wait_for_vertical_green, NULL);

  usleep(20000);
  TEST_ASSERT_FALSE_MESSAGE(wait_finished,
                            "A thread nao deveria atravessar enquanto o "
                            "semaforo vertical esta vermelho.");

  clock_start(10);
  usleep(70000);

  pthread_join(wait_thread, NULL);

  TEST_ASSERT_TRUE_MESSAGE(
      wait_finished,
      "A thread deveria ser liberada apos o semaforo trocar para verde.");
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_traffic_starts_horizontal_green);
  RUN_TEST(test_traffic_starts_vertical_red);
  RUN_TEST(test_traffic_ignores_non_intersection);
  RUN_TEST(test_traffic_wait_for_green_blocks_until_signal_changes);

  return UNITY_END();
}
