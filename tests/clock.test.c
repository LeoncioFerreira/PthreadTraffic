/**
 * Descrição: Testes unitários para o módulo de relógio.
 * Autor: Salomão
 */

#include "../src/modules/clock/clock.h"
#include "unity.h"
#include <unistd.h>

/* =========================================================================
   SETUP E TEARDOWN
   ========================================================================= */

void setUp(void) { clock_init(); }

void tearDown(void) { clock_destroy(); }

/* =========================================================================
   CASOS DE TESTE
   ========================================================================= */

void test_clock_initialization_starts_at_zero(void) {
  // Verifica se o init configurou o tick corretamente
  TEST_ASSERT_EQUAL_UINT64(0, global_tick);
}

void test_clock_ticks_increment_over_time(void) {
  // 1. Inicia o relógio com um tick de 10 milissegundos
  clock_start(10);

  // 2. Aguarda 35ms (tempo suficiente para o relógio "bater" umas 3 vezes)
  usleep(35000);

  // 3. Para o relógio para que possamos avaliar o resultado em paz
  clock_stop();

  // 4. Afirma (Assert) que o tick global tem que ser maior que 0
  TEST_ASSERT_TRUE_MESSAGE(
      global_tick > 0, "O tick global não incrementou após iniciar o relógio.");
}

void test_clock_synchronization_broadcasts_signal(void) {
  // Este teste simula o comportamento que um Veículo terá na simulação real.
  clock_start(10); // Tick rápido de 10ms

  pthread_mutex_lock(&clock_mutex);
  uint64_t initial_tick = global_tick;

  // A thread do teste (principal) vai "dormir" aguardando o sinal do relógio
  pthread_cond_wait(&clock_cond, &clock_mutex);

  // Se o código chegou até aqui, é porque o relógio deu um
  // pthread_cond_broadcast!
  uint64_t new_tick = global_tick;
  pthread_mutex_unlock(&clock_mutex);

  clock_stop();

  // Afirma que o tick mudou desde que fomos dormir
  TEST_ASSERT_TRUE_MESSAGE(
      new_tick > initial_tick,
      "O teste não foi acordado pelo broadcast do relógio.");
}

/* =========================================================================
   FUNÇÃO PRINCIPAL (RUNNER)
   ========================================================================= */

int main(void) {
  UNITY_BEGIN(); // Inicia o framework do Unity

  // Registra todos os testes que queremos executar
  RUN_TEST(test_clock_initialization_starts_at_zero);
  RUN_TEST(test_clock_ticks_increment_over_time);
  RUN_TEST(test_clock_synchronization_broadcasts_signal);

  return UNITY_END(); // Retorna o resultado final (quantos passaram/falharam)
}