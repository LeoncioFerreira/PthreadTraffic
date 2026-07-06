#include "../src/modules/display/display.h"
#include "../src/modules/clock/clock.h"
#include "../src/modules/map/map.h"
#include "../src/modules/traffic/traffic.h"
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

volatile bool keep_running = true;

static Map *test_map = NULL;
static int first_intersection_row = -1;
static int first_intersection_col = -1;
static bool traffic_started = false;

static void find_first_intersection(void) {
  first_intersection_row = -1;
  first_intersection_col = -1;

  for (int row = 0; row < test_map->rows; row++) {
    for (int col = 0; col < test_map->columns; col++) {
      if (test_map->cell_grid[row][col].type == INTERSECTION) {
        first_intersection_row = row;
        first_intersection_col = col;
        return;
      }
    }
  }
}

static int count_occurrences(const char *buffer, const char *pattern) {
  int count = 0;
  const char *cursor = buffer;

  while ((cursor = strstr(cursor, pattern)) != NULL) {
    count++;
    cursor += strlen(pattern);
  }

  return count;
}

void setUp(void) {
  clock_init();
  // Inicia o relógio rápido para a thread do display renderizar várias vezes
  traffic_started = false;
  test_map = load_map("mapa.txt");

  // <<< INICIALIZA O TRÁFEGO PARA O DISPLAY CONSEGUIR LER AS CORES >>>
  if (test_map) {
    find_first_intersection();
    traffic_init(test_map, 5);
  }
}

void tearDown(void) {
  // <<< PARA E DESTRÓI O TRÁFEGO >>>
  if (traffic_started) {
    traffic_stop();
  }
  if (clock_is_running()) {
    clock_stop();
  }
  traffic_destroy();

  if (test_map)
    destroy(test_map);
  clock_destroy();
}

void test_display_start_and_stop_safely(void) {
  TEST_ASSERT_NOT_NULL_MESSAGE(test_map,
                               "O mapa precisa carregar para o teste.");
  clock_start(10);
  traffic_start();
  traffic_started = true;

  // Inicia o display
  display_start(test_map);
  // Dorme por 50ms (o suficiente para o relógio emitir ticks e o display
  // renderizar)
  usleep(50000);
  // Para o display
  display_stop();

  // Se o código chegou até aqui sem "crashar" (Segmentation Fault), o teste
  // passou!
  TEST_ASSERT_TRUE_MESSAGE(true, "Display rodou e parou de forma segura.");
}

void test_display_shows_priority_alert_only_once_per_frame(void) {
  TEST_ASSERT_NOT_NULL_MESSAGE(test_map,
                               "O mapa precisa carregar para o teste.");
  TEST_ASSERT_TRUE_MESSAGE(first_intersection_row >= 0,
                           "O mapa precisa ter pelo menos um cruzamento.");

  FILE *capture = tmpfile();
  TEST_ASSERT_NOT_NULL_MESSAGE(capture, "Falha ao criar arquivo temporario.");

  fflush(stdout);
  int original_stdout = dup(STDOUT_FILENO);
  TEST_ASSERT_TRUE_MESSAGE(original_stdout >= 0,
                           "Falha ao duplicar stdout original.");
  TEST_ASSERT_EQUAL_INT_MESSAGE(STDOUT_FILENO,
                                dup2(fileno(capture), STDOUT_FILENO),
                                "Falha ao redirecionar stdout.");
  setvbuf(stdout, NULL, _IONBF, 0);

  traffic_request_priority(first_intersection_row, first_intersection_col, 'L');
  display_start(test_map);
  usleep(10000);

  pthread_mutex_lock(&clock_mutex);
  global_tick = 1;
  pthread_cond_broadcast(&clock_cond);
  pthread_mutex_unlock(&clock_mutex);

  usleep(20000);
  display_stop();
  traffic_release_priority(first_intersection_row, first_intersection_col);

  fflush(stdout);
  TEST_ASSERT_EQUAL_INT_MESSAGE(STDOUT_FILENO,
                                dup2(original_stdout, STDOUT_FILENO),
                                "Falha ao restaurar stdout.");
  setvbuf(stdout, NULL, _IONBF, 0);
  close(original_stdout);

  rewind(capture);

  char buffer[8192];
  size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, capture);
  buffer[bytes_read] = '\0';
  fclose(capture);

  TEST_ASSERT_EQUAL_INT_MESSAGE(1, count_occurrences(buffer, "[ALERTA]"),
                                "O display deve imprimir o alerta de "
                                "prioridade apenas uma vez por frame.");
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_display_start_and_stop_safely);
  RUN_TEST(test_display_shows_priority_alert_only_once_per_frame);
  return UNITY_END();
}
