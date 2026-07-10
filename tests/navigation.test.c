#include "../src/modules/navigation/navigation.h"
#include "unity.h"
#include <stdbool.h>
#include <stdlib.h>

volatile bool keep_running = true;

void setUp() {}
void tearDown() {}

Map *create_test_map(int rows, int cols) {
  Map *map = (Map *)malloc(sizeof(Map));
  map->rows = rows;
  map->columns = cols;
  map->cell_grid = (Cell **)malloc(rows * sizeof(Cell *));
  for (int i = 0; i < rows; i++) {
    map->cell_grid[i] = (Cell *)malloc(cols * sizeof(Cell));
    for (int j = 0; j < cols; j++) {
      map->cell_grid[i][j].type = EMPTY;
      map->cell_grid[i][j].direction = ' ';
      pthread_mutex_init(&map->cell_grid[i][j].mutex, NULL);
      map->cell_grid[i][j].current_vehicle = NULL;
    }
  }
  return map;
}

void free_test_map(Map *map) {
  for (int i = 0; i < map->rows; i++) {
    for (int j = 0; j < map->columns; j++) {
      pthread_mutex_destroy(&map->cell_grid[i][j].mutex);
    }
    free(map->cell_grid[i]);
  }
  free(map->cell_grid);
  free(map);
}

void test_navigation_returns_char() {
  Map *map = create_test_map(1, 1);
  map->cell_grid[0][0].direction = '>';
  map->cell_grid[0][0].type = ROADS;

  NavigationState state = {0, 0, '>', '>', 1};
  char dir = navigation_choose_direction(map, &state, 1);
  TEST_ASSERT_EQUAL_CHAR('>', dir);

  free_test_map(map);
}

void test_navigation_normal_road_cell() {
  Map *map = create_test_map(3, 3);
  map->cell_grid[1][1].direction = 'S';
  map->cell_grid[1][1].type = ROADS;

  NavigationState state = {1, 1, 'L', 'L', 1};
  char dir = navigation_choose_direction(map, &state, 5);
  TEST_ASSERT_EQUAL_CHAR('S', dir);

  free_test_map(map);
}

void test_navigation_non_intersection_empty() {
  Map *map = create_test_map(3, 3);
  map->cell_grid[1][1].direction = ' ';
  map->cell_grid[1][1].type = EMPTY;

  NavigationState state = {1, 1, 'L', 'O', 1};
  char dir = navigation_choose_direction(map, &state, 5);
  TEST_ASSERT_EQUAL_CHAR('O', dir); // should return last_valid_dir

  free_test_map(map);
}

void test_navigation_intersection_prioritizes_straight() {
  // 5x5 map with intersection in the middle (2,2)
  Map *map = create_test_map(5, 5);
  map->cell_grid[2][2].direction = ' ';
  map->cell_grid[2][2].type = INTERSECTION;

  // No other roads near the intersection.
  // Straight dir is 'L'. Random choice should pick it since only 'L' is in
  // valid_turns.
  NavigationState state = {2, 2, 'L', 'L', 1};
  char dir = navigation_choose_direction(map, &state, 1);
  TEST_ASSERT_EQUAL_CHAR('L', dir);

  free_test_map(map);
}

void test_navigation_intersection_radar_north() {
  // 5x5 map with intersection in the middle (2,2)
  Map *map = create_test_map(5, 5);
  map->cell_grid[2][2].direction = ' ';
  map->cell_grid[2][2].type = INTERSECTION;

  // Road pointing North, 2 cells North: (0, 2) has direction 'N'
  map->cell_grid[0][2].direction = 'N';
  map->cell_grid[0][2].type = ROADS;

  // Vehicle going East 'L'. North is valid, South not checked since current is
  // East. Straight is 'L' (2 entries). North is '^' (1 entry). Let's force
  // random choice to pick North. valid_turns will contain: ['L', 'L', '^']
  // tick=1, vehicle_id=1. choice = (rand() + 1 + 1) % 3.
  // Let's check that we can select either 'L' or '^' and no other characters.
  NavigationState state = {2, 2, 'L', 'L', 1};

  bool found_straight = false;
  bool found_north = false;

  for (uint64_t tick = 0; tick < 100; tick++) {
    char dir = navigation_choose_direction(map, &state, tick);
    if (dir == 'L') {
      found_straight = true;
    } else if (dir == '^') {
      found_north = true;
    } else {
      TEST_FAIL_MESSAGE("Invalid direction returned by choose_direction");
    }
  }

  TEST_ASSERT_TRUE(found_straight);
  TEST_ASSERT_TRUE(found_north);

  free_test_map(map);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_navigation_returns_char);
  RUN_TEST(test_navigation_normal_road_cell);
  RUN_TEST(test_navigation_non_intersection_empty);
  RUN_TEST(test_navigation_intersection_prioritizes_straight);
  RUN_TEST(test_navigation_intersection_radar_north);
  return UNITY_END();
}
