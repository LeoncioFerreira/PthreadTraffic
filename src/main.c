#define _GNU_SOURCE // CRUCIAL: Ativa o pthread_tryjoin_np no Linux/WSL
#include "modules/clock/clock.h"
#include "modules/display/display.h"
#include "modules/logger/logger.h"
#include "modules/map/map.h"
#include "modules/spawner/spawner.h"
#include "modules/traffic/traffic.h"
#include "modules/vehicle/vehicle.h"
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

typedef struct {
  int max_vehicles;
  int tick_ms;
  char *map_path;
} Config;

/* Esta variável controla se o simulador continua rodando ou não */
static volatile bool keep_running = true;

/* Função chamada automaticamente quando o usuário aperta Ctrl+C no terminal */
static void handle_shutdown_signal(int signal) {
  if (signal == SIGINT) {
    printf("\n[MAIN] Ctrl+C detectado! Preparando o encerramento do "
           "simulador...\n");
    keep_running = false;
  }
}

/* WRAPPER 1: Retorna true se todos os módulos iniciarem com sucesso. */
static bool init_systems(int argc, char *argv[], Config *cfg, Map **mapa) {
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

int main(int argc, char *argv[]) {
  Map *mapa = NULL;
  Config cfg = {.max_vehicles = 15, .tick_ms = 1000, .map_path = "mapa.txt"};

  /* Configura o tratamento do sinal Ctrl+C */
  signal(SIGINT, handle_shutdown_signal);

  /* Semeia o gerador de números aleatórios */
  srand(time(NULL));

  /* Executa a inicialização encapsulada */
  if (!init_systems(argc, argv, &cfg, &mapa)) {
    return EXIT_FAILURE;
  }

  /* Dispara os motores do relógio e dos semáforos */
  clock_start(cfg.tick_ms);
  traffic_start();

  if (!spawner_start(cfg.max_vehicles, mapa)) {
    fprintf(stderr, "[MAIN] ERRO: Falha ao inicializar o spawner.\n");
    clock_stop();
    traffic_stop();
    clock_destroy();
    traffic_destroy();
    destroy(mapa);
    logger_destroy();
    return EXIT_FAILURE;
  }

  printf("[MAIN] Ativando a thread de Visualização (Display)...\n");
  display_start(mapa);

  printf("[MAIN] Simulação dinâmica em andamento. Use Ctrl+C para sair.\n");

  while (keep_running == true) {
    sleep(1);
  }

  printf("[MAIN] Encerrando recursos de forma segura...\n");

  // Para os motores para forçar as threads restantes a sair dos seus loops de
  // tick
  clock_stop();
  traffic_stop();
  display_stop();

  spawner_stop();

  clock_destroy();
  traffic_destroy();
  if (mapa) {
    destroy(mapa);
  }

  printf("[MAIN] Fechando descritores de logs...\n");
  logger_write(LOG_INFO, "Simulador de Tráfego encerrado de forma limpa.");
  logger_destroy();

  printf("[MAIN] Sistema finalizado com sucesso!\n");
  return EXIT_SUCCESS;
}