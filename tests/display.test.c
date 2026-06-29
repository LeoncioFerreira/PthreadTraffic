#include "../src/modules/display/display.h"
#include "../src/modules/clock/clock.h"
#include "../src/modules/map/map.h"
#include "../src/modules/traffic/traffic.h"
#include "unity.h"
#include <unistd.h>

volatile bool keep_running = true;

static Map *test_map = NULL;

void setUp(void) {
  clock_init();
  // Inicia o relógio rápido para a thread do display renderizar várias vezes
  clock_start(10);
  test_map = load_map("mapa.txt");

  // <<< INICIALIZA O TRÁFEGO PARA O DISPLAY CONSEGUIR LER AS CORES >>>
  if (test_map) {
    traffic_init(test_map, 5);
    traffic_start();
  }
}

void tearDown(void) {
  // <<< PARA E DESTRÓI O TRÁFEGO >>>
  traffic_stop();
  traffic_destroy();

  if (test_map)
    destroy(test_map);
  clock_stop();
  clock_destroy();
}

void test_display_start_and_stop_safely(void) {
  TEST_ASSERT_NOT_NULL_MESSAGE(test_map,
                               "O mapa precisa carregar para o teste.");

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

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_display_start_and_stop_safely);
  return UNITY_END();
}