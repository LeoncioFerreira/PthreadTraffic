#define _GNU_SOURCE // CRUCIAL: Ativa o pthread_tryjoin_np no Linux/WSL
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

#define MAX_CONCURRENT_VEHICLES                                                \
  20                          // Limite máximo de carros simultâneos no mapa
#define SPAWN_INTERVAL_SECS 2 // Tenta spawnar um carro a cada X segundos

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
    fprintf(stderr, "[WRAPPER] ERRO: Falha crítica ao carregar o mapa.\n");
    return false;
  }

  /* 2. Inicializa o relógio global */
  clock_init();

  /* 3. Inicializa o subsistema de semáforos (ritmo de 5 ticks) */
  traffic_init(*mapa, 5);

  return true;
}

/* Função auxiliar para encontrar um ponto de spawn válido e seguro no mapa */
static bool find_random_spawn_position(const Map *map, int *out_row,
                                       int *out_col) {
  int attempts =
      100; // Limite para evitar loop infinito caso o mapa esteja lotado

  while (attempts-- > 0) {
    int r = rand() % map->rows;
    int c = map->columns > 0 ? rand() % map->columns : 0;

    // Regra de segurança: Deve ser uma pista válida e NÃO pode ser um
    // cruzamento
    if (map->cell_grid[r][c].type != INTERSECTION &&
        map->cell_grid[r][c].direction != ' ') {
      *out_row = r;
      *out_col = c;
      return true;
    }
  }
  return false;
}

int main(void) {
  Map *mapa = NULL;
  unsigned int vehicle_id_counter = 1;

  /* Configura o tratamento do sinal Ctrl+C */
  signal(SIGINT, handle_shutdown_signal);

  /* Semeia o gerador de números aleatórios */
  srand(time(NULL));

  /* Executa a inicialização encapsulada */
  if (!init_systems(&mapa)) {
    return EXIT_FAILURE;
  }

  /* Dispara os motores do relógio e dos semáforos */
  clock_start(200);
  traffic_start();

  // Pool dinâmica de veículos inicialmente vazia
  Vehicle *fleet[MAX_CONCURRENT_VEHICLES];
  for (int i = 0; i < MAX_CONCURRENT_VEHICLES; i++) {
    fleet[i] = NULL;
  }

  printf("[MAIN] Ativando a thread de Visualização (Display)...\n");
  display_start(mapa);

  printf("[MAIN] Simulação dinâmica em andamento. Use Ctrl+C para sair.\n");

  int spawn_timer = 0;

  /* Loop principal de execução atuando como Gerenciador/Spawner */
  while (keep_running == true) {

    // Garbage Collection de Threads: percorre a pool procurando por veículos
    // que saíram do mapa para libertar recursos
    for (int i = 0; i < MAX_CONCURRENT_VEHICLES; i++) {
      if (fleet[i] != NULL) {
        // pthread_tryjoin_np tenta fazer o join. Se retornar 0, a thread já
        // terminou.
        if (pthread_tryjoin_np(fleet[i]->thread_id, NULL) == 0) {
          vehicle_destroy(fleet[i]);
          fleet[i] = NULL;
        }
      }
    }

    // PASSO 2: Spawner Automático Dinâmico
    spawn_timer++;
    if (spawn_timer >= SPAWN_INTERVAL_SECS) {
      spawn_timer = 0;

      // Procura por um slot livre na pool
      int free_slot = -1;
      for (int i = 0; i < MAX_CONCURRENT_VEHICLES; i++) {
        if (fleet[i] == NULL) {
          free_slot = i;
          break;
        }
      }

      // Se houver vaga na pool, tenta spawnar
      if (free_slot != -1) {
        int spawn_row, spawn_col;

        if (find_random_spawn_position(mapa, &spawn_row, &spawn_col)) {
          // Sorteia o tipo do carro
          VehicleType type;
          int speed_ticks;
          int roll = rand() % 4;

          if (roll == 0) {
            type = FAST_CAR;
            speed_ticks = 1;
          } else if (roll == 1) {
            type = AMBULANCE;
            speed_ticks = 1;
          } else if (roll == 2) {
            type = MEDIUM_CAR;
            speed_ticks = 2;
          } else {
            type = SLOW_CAR;
            speed_ticks = 4;
          }

          // Cria e inicia a thread do veículo
          Vehicle *new_v =
              vehicle_create_and_start(vehicle_id_counter++, spawn_row,
                                       spawn_col, speed_ticks, type, mapa);
          if (new_v != NULL) {
            fleet[free_slot] = new_v;
          }
        }
      }
    }

    sleep(1);
  }

  printf("[MAIN] Encerrando recursos de forma segura...\n");

  // Para os motores para forçar as threads restantes a sair dos seus loops de
  // tick
  clock_stop();
  traffic_stop();
  display_stop();

  printf("[MAIN] Finalizando as threads dos veículos restantes...\n");
  for (int i = 0; i < MAX_CONCURRENT_VEHICLES; i++) {
    if (fleet[i] != NULL) {
      pthread_join(fleet[i]->thread_id, NULL);
      vehicle_destroy(fleet[i]);
    }
  }

  clock_destroy();
  traffic_destroy();
  if (mapa) {
    destroy(mapa);
  }

  printf("[MAIN] Sistema finalizado com sucesso!\n");
  return EXIT_SUCCESS;
}