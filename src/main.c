/**
 * Descrição: Orquestração do sistema, inicialização dos subsistemas, disparo
 * das threads iniciais e garantia do encerramento seguro de todo o simulador.
 * Autor: André
 */

#include "modules/clock/clock.h"
#include "modules/map/map.h"
#include "modules/traffic/traffic.h"
#include "modules/vehicle/vehicle.h"
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
static bool init_systems(Map **mapa) {
  printf("[WRAPPER] Inicializando subsistemas...\n");

  /* 1. Inicializa o mapa */
  *mapa = load_map("mapa.txt");
  if (*mapa == NULL) {
    fprintf(stderr, "[MAIN] ERRO: Falha no carregamento do mapa.\n");
    return false;
  }
  traffic_init(*mapa, 5);

  /* 2. Inicializa as estruturas do Relógio (Mutex e Condição) */
  clock_init();
  printf("[WRAPPER] Subsistema de Relógio inicializado com sucesso.\n");

  return true;
}

int main() {
  /* Direciona o Ctrl+C para a função */
  signal(SIGINT, handle_shutdown_signal);

  printf("Simulador de Tráfego Pthread\n");

  /* Carregamento e inicialização dos sistemas através do Wrapper 1 */
  Map *mapa = NULL;
  if (init_systems(&mapa) == false) {
    return EXIT_FAILURE;
  }

  printf("Mapa carregado com sucesso! Dimensões: %d linhas x %d colunas\n",
         mapa->rows, mapa->columns);

  /* Dispara a thread do Relógio */
  printf("[MAIN] Ativando a thread do Relógio...\n");
  clock_start(1000);
  traffic_start();

  /* Cria e dispara o Veículo de teste */
  printf("[MAIN] Criando e ativando veículo de teste...\n");
  Vehicle *test_vehicle = vehicle_create_and_start(1, 0, 0, 1, 0, mapa);
  if (test_vehicle == NULL) {
    fprintf(stderr, "[MAIN] ERRO: Falha ao criar o veículo de teste.\n");
    clock_stop();
    clock_destroy();
    destroy(mapa);
    return EXIT_FAILURE;
  }

  printf("[MAIN] Simulação em andamento. Use Ctrl+C para sair.\n");

  /* Loop principal de execução */
  while (keep_running == true) {
    sleep(1);
  }

  printf("[MAIN] Encerrando recursos de forma segura...\n");

  if (test_vehicle != NULL) {
    printf(
        "[MAIN] Finalizando a thread do veículo de teste de forma segura...\n");
    vehicle_destroy(test_vehicle);
  }

  printf("[MAIN] Parando e destruindo o subsistema de semáforos...\n");
  traffic_stop();
  traffic_destroy();

  printf("[MAIN] Parando e destruindo o relógio...\n");
  clock_stop();
  clock_destroy();

  printf("[MAIN] Destruindo a estrutura do mapa...\n");
  destroy(mapa);

  printf("[MAIN] Simulação encerrada com sucesso!\n");
  return EXIT_SUCCESS;
}