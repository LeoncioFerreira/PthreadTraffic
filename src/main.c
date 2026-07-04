/**
 * Descrição: Orquestração do sistema, inicialização dos subsistemas, disparo
 * das threads iniciais e garantia do encerramento seguro de todo o simulador.
 * Autor: André
 */

#include "modules/clock/clock.h"
#include "modules/display/display.h"
#include "modules/map/map.h"
#include "modules/traffic/traffic.h"
#include "modules/vehicle/vehicle.h"
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define NUM_VEHICLES 12

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

  srand(time(NULL));
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

  Vehicle *fleet[NUM_VEHICLES];

  // Coordenadas (linha, coluna) de ruas válidas nas bordas do seu mapa atual
  // Ex: Linha 3 (Leste), Linha 8 (Leste), Linha 13 (Leste), Linha 4 (Oeste),
  // Linha 9 (Oeste)
  const int start_positions[NUM_VEHICLES][2] = {
      {3, 0},   // Leste (linha 3)
      {4, 42},  // Oeste (linha 4)
      {8, 0},   // Leste (linha 8)
      {9, 42},  // Oeste (linha 9)
      {13, 0},  // Leste (linha 13)
      {14, 42}, // Oeste (linha 14)
      {17, 7},  // Norte (coluna 7)
      {0, 8},   // Sul (coluna 8)
      {17, 16}, // Norte (coluna 16)
      {0, 17},  // Sul (coluna 17)
      {17, 25}, // Norte (coluna 25)
      {0, 26}   // Sul (coluna 26)
  };

  for (int i = 0; i < NUM_VEHICLES; i++) {
    VehicleType type = FAST_CAR;
    int speed_ticks = 1;

    if (i <= 4) {
      type = FAST_CAR;
      speed_ticks = 1;
    } else if (i > 4 && i <= 8) {
      type = MEDIUM_CAR;
      speed_ticks = 2;
    } else {
      type = SLOW_CAR;
      speed_ticks = 4;
    }

    fleet[i] = vehicle_create_and_start(i + 1, start_positions[i][0],
                                        start_positions[i][1], speed_ticks,
                                        type, mapa);

    if (fleet[i] == NULL) {
      fprintf(stderr, "[MAIN] ERRO: Falha ao criar o veículo %d.\n", i + 1);
      // Num cenário real faríamos um loop de limpeza aqui, mas vamos seguir
    }
  }

  printf("[MAIN] Simulação em andamento. Use Ctrl+C para sair.\n");

  printf("[MAIN] Ativando a thread de Visualização (Display)...\n");
  display_start(mapa);

  printf("[MAIN] Simulação em andamento. Use Ctrl+C para sair.\n");

  /* Loop principal de execução */
  while (keep_running == true) {
    sleep(1);
  }

  printf("[MAIN] Encerrando recursos de forma segura...\n");
  clock_stop();

  traffic_stop();

  display_stop();

  printf(
      "[MAIN] Finalizando a thread do veículo de teste de forma segura...\n");
  for (int i = 0; i < NUM_VEHICLES; i++) {
    if (fleet[i] != NULL) {
      vehicle_destroy(fleet[i]);
    }
  }

  printf("[MAIN] Parando e destruindo o subsistema de semáforos...\n");
  traffic_destroy();

  printf("[MAIN] Parando e destruindo o relógio...\n");
  clock_destroy();

  printf("[MAIN] Destruindo a estrutura do mapa...\n");
  destroy(mapa);

  printf("[MAIN] Simulação encerrada com sucesso!\n");
  return EXIT_SUCCESS;
}
