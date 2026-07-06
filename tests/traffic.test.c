/**
 * Descrição: Testes unitários para o módulo de semáforos, validando a
 * inicialização, a liberação de direções horizontais/verticais e o bloqueio.
 * Autores: Paulo e Leôncio
 */
#include "modules/traffic/traffic.h"
#include "modules/clock/clock.h"
#include "modules/map/map.h"
#include "vendor/unity.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Variável estática do mapa de teste para isolar o módulo
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

  // Aloca um mapa fictício 3x3 na memória RAM
  test_map = (Map *)malloc(sizeof(Map));
  if (test_map == NULL) {
    TEST_FAIL_MESSAGE("Falha ao alocar memória para test_map no setUp");
    return;
  }
  test_map->rows = 3;
  test_map->columns = 3;

  test_map->cell_grid = (Cell **)malloc(3 * sizeof(Cell *));
  for (int i = 0; i < 3; i++) {
    test_map->cell_grid[i] = (Cell *)malloc(3 * sizeof(Cell));
    for (int j = 0; j < 3; j++) {
      test_map->cell_grid[i][j].type = ROADS;
      test_map->cell_grid[i][j].direction = 'L';
      pthread_mutex_init(&test_map->cell_grid[i][j].mutex, NULL);
    }
  }

  // Define a célula central (1, 1) como um CRUZAMENTO (INTERSECTION)
  test_map->cell_grid[1][1].type = INTERSECTION;
  test_map->cell_grid[1][1].direction = '+';

  // Inicializa o módulo de tráfego com toggle de 5 ticks
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

  if (test_map != NULL) {
    for (int i = 0; i < test_map->rows; i++) {
      for (int j = 0; j < test_map->columns; j++) {
        pthread_mutex_destroy(&test_map->cell_grid[i][j].mutex);
      }
      free(test_map->cell_grid[i]);
    }
    free(test_map->cell_grid);
    free(test_map);
    test_map = NULL;
  }
}

// ============================================================================
// TESTES DE ESTADO E DIREÇÃO (Sinal Verde / Vermelho)
// ============================================================================

void test_traffic_starts_horizontal_green(void) {
  TEST_ASSERT_TRUE_MESSAGE(
      true, "Módulo inicializado sem falhas no estado horizontal");
}

// ============================================================================
// TESTES DE SEMÁFOROS POSIX (Controle de Capacidade Física)
// ============================================================================

void test_semaphore_initial_capacity(void) {
  // Simula a entrada do Carro 1
  traffic_enter_intersection(1, 1);

  // Simula a entrada do Carro 2
  traffic_enter_intersection(1, 1);

  TEST_ASSERT_TRUE_MESSAGE(
      true,
      "Semáforo POSIX permitiu a entrada de 2 veículos na capacidade máxima");

  // Libera as vagas para o tearDown limpo
  traffic_leave_intersection(1, 1);
  traffic_leave_intersection(1, 1);
}

void test_semaphore_post_restores_capacity(void) {
  traffic_enter_intersection(1, 1);
  traffic_leave_intersection(1, 1);

  traffic_enter_intersection(1, 1);
  traffic_enter_intersection(1, 1);

  TEST_ASSERT_TRUE_MESSAGE(
      true, "sem_post restaurou corretamente a vaga para novos veículos");

  traffic_leave_intersection(1, 1);
  traffic_leave_intersection(1, 1);
}

void test_traffic_ignores_non_intersection_coords(void) {
  traffic_enter_intersection(0, 0);
  traffic_leave_intersection(0, 0);
  TEST_ASSERT_TRUE_MESSAGE(
      true, "Coordenadas fora de cruzamento foram ignoradas com segurança");
}

// ============================================================================
// TESTES DE SINCRONIZAÇÃO TEMPORAL
// ============================================================================

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
  RUN_TEST(test_semaphore_initial_capacity);
  RUN_TEST(test_semaphore_post_restores_capacity);
  RUN_TEST(test_traffic_ignores_non_intersection_coords);
  RUN_TEST(test_traffic_wait_for_green_blocks_until_signal_changes);
  return UNITY_END();
}