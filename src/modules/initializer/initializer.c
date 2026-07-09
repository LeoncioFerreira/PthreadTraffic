#define _GNU_SOURCE
#include "initializer.h"
#include "../clock/clock.h"
#include "../logger/logger.h"
#include "../traffic/traffic.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile sig_atomic_t keep_running = 1;

static void handle_shutdown_signal(int signal) {
  if (signal == SIGINT) {
    printf("\n[MAIN] Ctrl+C detectado! Preparando o encerramento do "
           "simulador...\n");
    keep_running = 0;
  }
}

bool system_init(int argc, char *argv[], Config *cfg, Map **mapa) {
  // Configura default
  cfg->max_vehicles = 15;
  cfg->tick_ms = 1000;
  cfg->map_path = "mapa.txt";

  int opt;
  while ((opt = getopt(argc, argv, "v:t:m:")) != -1) {
    switch (opt) {
    case 'v':
      cfg->max_vehicles = atoi(optarg);
      break;
    case 't':
      cfg->tick_ms = atoi(optarg);
      break;
    case 'm':
      cfg->map_path = optarg;
      break;
    default:
      fprintf(stderr, "Uso: %s [-v veiculos] [-t tick_ms] [-m mapa.txt]\n",
              argv[0]);
      return false;
    }
  }

  /* Configura o tratamento do sinal Ctrl+C */
  signal(SIGINT, handle_shutdown_signal);

  printf("[WRAPPER] Inicializando subsistemas...\n");

  /* 0. Inicializa o subsistema de Logs concorrente antes de qualquer outro */
  if (!logger_init("simulador.log")) {
    fprintf(stderr, "[MAIN] ERRO: Falha ao inicializar o arquivo de logs.\n");
    return false;
  }
  logger_write(LOG_INFO, "Simulador de Tráfego Pthread iniciado.");

  /* 1. Inicializa o mapa */
  *mapa = load_map(cfg->map_path);
  if (*mapa == NULL) {
    fprintf(stderr, "[WRAPPER] ERRO: Falha crítica ao carregar o mapa.\n");
    logger_destroy();
    return false;
  }

  /* 2. Inicializa o relógio global */
  clock_init();

  /* 3. Inicializa o subsistema de semáforos (ritmo de 5 ticks) */
  traffic_init(*mapa, 5);

  return true;
}

void system_wait_for_shutdown(void) {
  while (keep_running == 1) {
    sleep(1);
  }
}

void system_cleanup(void) {
  printf("[MAIN] Fechando descritores de logs...\n");
  logger_write(LOG_INFO, "Simulador de Tráfego encerrado de forma limpa.");
  logger_destroy();
  printf("[MAIN] Sistema finalizado com sucesso!\n");
}
