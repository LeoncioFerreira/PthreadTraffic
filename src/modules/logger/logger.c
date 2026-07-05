/**
 * Descrição: Implementação do subsistema de logging síncrono e thread-safe.
 * Protege o arquivo de saída utilizando exclusão mútua por mutex.
 * Autor: André
 */

#include "logger.h"
#include "../clock/clock.h"
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>

static FILE *log_file = NULL;
static pthread_mutex_t logger_mutex;
static bool initialized = false;

bool logger_init(const char *filename) {
  if (initialized)
    return true;

  log_file = fopen(filename, "w"); // Abre e limpa o arquivo anterior
  if (log_file == NULL) {
    return false;
  }

  if (pthread_mutex_init(&logger_mutex, NULL) != 0) {
    fclose(log_file);
    return false;
  }

  initialized = true;
  return true;
}

void logger_write(LogLevel level, const char *format, ...) {
  if (!initialized || log_file == NULL)
    return;

  pthread_mutex_lock(&logger_mutex);

  // Captura o tick de forma segura usando o mutex do clock que já está exposto
  pthread_mutex_lock(&clock_mutex);
  uint64_t current_tick = global_tick;
  pthread_mutex_unlock(&clock_mutex);

  // Escreve o prefixo do tempo e severidade
  if (level == LOG_ALERT) {
    fprintf(log_file, "[Tick %lu] [ALERTA] ", current_tick);
  } else {
    fprintf(log_file, "[Tick %lu] ", current_tick);
  }

  // Processa os argumentos dinâmicos (estilo printf)
  va_list args;
  va_start(args, format);
  vfprintf(log_file, format, args);
  va_end(args);

  fprintf(log_file, "\n");
  fflush(log_file); // Garante a escrita física imediata em disco

  pthread_mutex_unlock(&logger_mutex);
}

void logger_destroy(void) {
  if (!initialized)
    return;

  pthread_mutex_lock(&logger_mutex);
  if (log_file != NULL) {
    fclose(log_file);
    log_file = NULL;
  }
  initialized = false;
  pthread_mutex_unlock(&logger_mutex);

  pthread_mutex_destroy(&logger_mutex);
}