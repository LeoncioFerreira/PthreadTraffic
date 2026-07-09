#define _GNU_SOURCE
#include "modules/clock/clock.h"
#include "modules/display/display.h"
#include "modules/initializer/initializer.h"
#include "modules/map/map.h"
#include "modules/spawner/spawner.h"
#include "modules/traffic/traffic.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char *argv[]) {
  Map *mapa = NULL;
  Config cfg;

  /* Semeia o gerador de números aleatórios */
  srand(time(NULL));

  /* Executa a inicialização de todos os subsistemas encapsulados */
  if (!system_init(argc, argv, &cfg, &mapa)) {
    return EXIT_FAILURE;
  }

  /* Exibe a legenda inicial da simulação com contagem regressiva */
  display_print_legend();

  /* Dispara os motores do relógio e dos semáforos */
  clock_start(cfg.tick_ms);
  traffic_start();

  if (!spawner_start(cfg.max_vehicles, mapa)) {
    fprintf(stderr, "[MAIN] ERRO: Falha ao inicializar o spawner.\n");
    clock_stop();
    traffic_stop();
    clock_destroy();
    traffic_destroy();
    if (mapa) {
      destroy(mapa);
    }
    system_cleanup();
    return EXIT_FAILURE;
  }

  printf("[MAIN] Ativando a thread de Visualização (Display)...\n");
  display_start(mapa);

  printf("[MAIN] Simulação dinâmica em andamento. Use Ctrl+C para sair.\n");

  /* Aguarda o sinal de encerramento (Ctrl+C) de forma passiva */
  system_wait_for_shutdown();

  printf("[MAIN] Encerrando recursos de forma segura...\n");
  fflush(stdout);

  // 1. Para a visualização (Display) primeiro para evitar flicker/limpeza de
  // tela durante a saída
  display_stop();

  // 2. Para a geração de novos veículos antes de parar o relógio e o tráfego
  spawner_stop();

  // 3. Para os motores para forçar as threads restantes a sair dos seus loops
  // de tick
  clock_stop();
  traffic_stop();

  printf("[MAIN] Finalizando as threads dos veículos restantes...\n");
  fflush(stdout);

  // 4. Aguarda e limpa as threads de veículos que saíram de forma segura
  spawner_cleanup();

  /* Destrói e libera os recursos dos subsistemas */
  clock_destroy();
  traffic_destroy();
  if (mapa) {
    destroy(mapa);
  }

  /* Finaliza o logging e encerra o sistema formalmente */
  system_cleanup();

  return EXIT_SUCCESS;
}