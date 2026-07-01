/**
 * Descrição: Implementação do módulo de relógio. Contém a thread principal do
 * relógio que notifica ativamente (via broadcast) todas as threads
 * dependentes a cada "tick".
 */

#include "clock.h"
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

uint64_t global_tick = 0;
pthread_mutex_t clock_mutex;
pthread_cond_t clock_cond;

static pthread_t clock_thread;
static bool is_running = false;
static int tick_interval_ms = 1000;

/**
 * Rotina executada pela thread do relógio.
 */
static void *clock_routine(void *arg) {
  (void)arg;
  struct timespec ts;

  while (1) {
    // Configura o tempo de dormência (sleep)
    ts.tv_sec = tick_interval_ms / 1000;
    ts.tv_nsec = (tick_interval_ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);

    pthread_mutex_lock(&clock_mutex);

    // Verifica se o relógio deve ser desligado
    if (!is_running) {
      pthread_mutex_unlock(&clock_mutex);
      break;
    }

    // Atualiza o tick e notifica o restante do sistema
    global_tick++;
    pthread_cond_broadcast(&clock_cond);

    pthread_mutex_unlock(&clock_mutex);
  }

  return NULL;
}

void clock_init(void) {
  global_tick = 0;
  pthread_mutex_init(&clock_mutex, NULL);
  pthread_cond_init(&clock_cond, NULL);
}

void clock_start(int interval_ms) {
  if (interval_ms > 0) {
    tick_interval_ms = interval_ms;
  }

  pthread_mutex_lock(&clock_mutex);
  is_running = true;
  pthread_mutex_unlock(&clock_mutex);

  pthread_create(&clock_thread, NULL, clock_routine, NULL);
}

void clock_stop(void) {
  pthread_mutex_lock(&clock_mutex);
  is_running = false;

  // Dispara um último broadcast para acordar veículos presos em
  // pthread_cond_wait e permitir que eles verifiquem condições de parada,
  // evitando deadlocks.
  pthread_cond_broadcast(&clock_cond);
  pthread_mutex_unlock(&clock_mutex);

  // Aguarda a finalização segura da thread
  pthread_join(clock_thread, NULL);
}

void clock_destroy(void) {
  pthread_mutex_destroy(&clock_mutex);
  pthread_cond_destroy(&clock_cond);
}

bool clock_is_running(void) {
  bool running;
  pthread_mutex_lock(&clock_mutex);
  running = is_running;
  pthread_mutex_unlock(&clock_mutex);
  return running;
}