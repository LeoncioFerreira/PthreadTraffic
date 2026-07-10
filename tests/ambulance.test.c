#include "../src/modules/ambulance/ambulance.h"
#include "../src/modules/map/map.h"
#include "../src/modules/traffic/traffic.h"
#include "../src/modules/vehicle/vehicle.h"
#include "vendor/unity.h"
#include <stdbool.h>
#include <stdlib.h>

static Map *test_map = NULL;
static Vehicle *test_ambulance = NULL;
static Vehicle *test_normal_car = NULL;

void setUp(void) {
  test_map = (Map *)malloc(sizeof(Map));
  test_map->rows = 3;
  test_map->columns = 3;
  test_map->cell_grid = (Cell **)malloc(3 * sizeof(Cell *));
  for (int i = 0; i < 3; i++) {
    test_map->cell_grid[i] = (Cell *)malloc(3 * sizeof(Cell));
    for (int j = 0; j < 3; j++) {
      test_map->cell_grid[i][j].type = EMPTY;
      test_map->cell_grid[i][j].direction = ' ';
    }
  }
  test_map->cell_grid[1][1].type = INTERSECTION;

  traffic_init(test_map, 5);

  test_ambulance = (Vehicle *)malloc(sizeof(Vehicle));
  test_ambulance->type = AMBULANCE;

  test_normal_car = (Vehicle *)malloc(sizeof(Vehicle));
  test_normal_car->type = FAST_CAR;
}

void tearDown(void) {
  traffic_stop();
  traffic_destroy();
  for (int i = 0; i < 3; i++) {
    free(test_map->cell_grid[i]);
  }
  free(test_map->cell_grid);
  free(test_map);
  free(test_ambulance);
  free(test_normal_car);
}

void test_ambulance_request_path_on_intersection(void) {
  TEST_ASSERT_FALSE_MESSAGE(traffic_is_priority_active(),
                            "Prioridade não deveria estar ativa no início");

  ambulance_request_path(test_ambulance, test_map, 1, 1, 'L');

  TEST_ASSERT_TRUE_MESSAGE(
      traffic_is_priority_active(),
      "Prioridade deve estar ativa após ambulância solicitar em cruzamento");
}

void test_ambulance_clear_path_on_intersection(void) {
  ambulance_request_path(test_ambulance, test_map, 1, 1, 'L');
  TEST_ASSERT_TRUE_MESSAGE(traffic_is_priority_active(), "Prioridade ativada");

  ambulance_clear_path(test_ambulance, test_map, 1, 1);

  TEST_ASSERT_FALSE_MESSAGE(
      traffic_is_priority_active(),
      "Prioridade deve ser desativada após ambulância limpar caminho");
}

void test_normal_car_cannot_request_path(void) {
  ambulance_request_path(test_normal_car, test_map, 1, 1, 'L');
  TEST_ASSERT_FALSE_MESSAGE(traffic_is_priority_active(),
                            "Carro normal não pode ativar prioridade");
}

void test_ambulance_request_on_regular_road(void) {
  ambulance_request_path(test_ambulance, test_map, 0, 0, 'L');
  TEST_ASSERT_FALSE_MESSAGE(
      traffic_is_priority_active(),
      "Ambulância não deve ativar prioridade fora de cruzamentos");
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_ambulance_request_path_on_intersection);
  RUN_TEST(test_ambulance_clear_path_on_intersection);
  RUN_TEST(test_normal_car_cannot_request_path);
  RUN_TEST(test_ambulance_request_on_regular_road);

  return UNITY_END();
}
