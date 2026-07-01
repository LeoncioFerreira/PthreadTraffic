#include "../src/modules/vehicle/vehicle.h"
#include "../src/modules/clock/clock.h"
#include "../src/modules/map/map.h"
#include "unity.h"
#include <stdbool.h>
#include <unistd.h>

volatile bool keep_running = true;

/**
 * Descrição: Testes unitários para o módulo de veículos. Valida a criação,
 * inicialização e o ciclo de vida da thread utilizando o mapa real e os ticks
 * do relógio.
 * Autor: Leôncio Ferreira
 */

Map *test_map = NULL;

void setUp() {
  clock_init();

  test_map = load_map("mapa.txt");
}

void tearDown() {
  if (test_map)
    destroy(test_map);
  clock_destroy();
}

void test_vehicle_creation_and_lifecycle_on_real_map() {
  TEST_ASSERT_NOT_NULL_MESSAGE(test_map,
                               "O mapa não foi carregado corretamente!");

  Vehicle *vehicle = vehicle_create_and_start(99, 0, 7, 1, FAST_CAR, test_map);

  TEST_ASSERT_NOT_NULL(vehicle);
  TEST_ASSERT_EQUAL_INT(99, vehicle->id);
  TEST_ASSERT_EQUAL_INT(1, vehicle->speed_ticks);

  usleep(50000);

  pthread_mutex_lock(&clock_mutex);
  global_tick = 1;
  pthread_cond_broadcast(&clock_cond);
  pthread_mutex_unlock(&clock_mutex);

  vehicle_destroy(vehicle);

  TEST_ASSERT_TRUE(true);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_vehicle_creation_and_lifecycle_on_real_map);
  return UNITY_END();
}
