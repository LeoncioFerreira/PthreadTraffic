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

#define MAX_CONCURRENT_VEHICLES 15
#define NUM_SPAWN_POINTS 12

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

static const int spawn_positions[NUM_SPAWN_POINTS][2] = {
    {3, 0},  {4, 42}, {8, 0},   {9, 42}, {13, 0},  {14, 42},
    {17, 7}, {0, 8},  {17, 16}, {0, 17}, {17, 25}, {0, 26}};

static pthread_mutex_t spawn_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t spawn_cond = PTHREAD_COND_INITIALIZER;
static int veiculos_ativos = 0;
static int ambulancias_ativas = 0;
static bool ambulance_spawned = false;

static Vehicle *fleet[MAX_CONCURRENT_VEHICLES];
static unsigned int vehicle_id_counter = 1;

static bool try_spawn_one(Map *mapa) {
  VehicleType type;
  int speed_ticks;

  if (!ambulance_spawned || ambulancias_ativas == 0) {
    type = AMBULANCE;
    speed_ticks = 1;
    ambulance_spawned = true;
  } else {
    int roll = rand() % 3;
    if (roll == 0) {
      type = FAST_CAR;
      speed_ticks = 1;
    } else if (roll == 1) {
      type = MEDIUM_CAR;
      speed_ticks = 2;
    } else {
      type = SLOW_CAR;
      speed_ticks = 4;
    }
  }

  int order[NUM_SPAWN_POINTS];
  for (int i = 0; i < NUM_SPAWN_POINTS; i++)
    order[i] = i;
  for (int i = NUM_SPAWN_POINTS - 1; i > 0; i--) {
    int j = rand() % (i + 1);
    int tmp = order[i];
    order[i] = order[j];
    order[j] = tmp;
  }

  for (int i = 0; i < NUM_SPAWN_POINTS; i++) {
    int idx = order[i];
    int row = spawn_positions[idx][0];
    int col = spawn_positions[idx][1];

    Vehicle *v = vehicle_create_and_start(vehicle_id_counter++, row, col,
                                          speed_ticks, type, mapa);
    if (v != NULL) {
      for (int s = 0; s < MAX_CONCURRENT_VEHICLES; s++) {
        if (fleet[s] == NULL) {
          fleet[s] = v;
          break;
        }
      }
      if (type == AMBULANCE)
        ambulancias_ativas++;
      return true;
    }
  }
  return false;
}

typedef struct {
  Map *mapa;
} SpawnArgs;

static void *spawn_manager_routine(void *arg) {
  Map *mapa = ((SpawnArgs *)arg)->mapa;

  while (keep_running) {
    pthread_mutex_lock(&spawn_mutex);

    // Garbage Collection de Threads: percorre a pool procurando por veículos
    // que saíram do mapa para libertar recursos
    for (int i = 0; i < MAX_CONCURRENT_VEHICLES; i++) {
      if (fleet[i] != NULL) {
        if (pthread_tryjoin_np(fleet[i]->thread_id, NULL) == 0) {
          if (fleet[i]->type == AMBULANCE)
            ambulancias_ativas--;
          free(fleet[i]);
          fleet[i] = NULL;
          veiculos_ativos--;
        }
      }
    }

    // PASSO 2: Spawner Automático Dinâmico
    while (veiculos_ativos < MAX_CONCURRENT_VEHICLES && keep_running) {
      if (try_spawn_one(mapa)) {
        veiculos_ativos++;
      } else {
        break;
      }
    }

    if (veiculos_ativos < MAX_CONCURRENT_VEHICLES) {
      pthread_mutex_unlock(&spawn_mutex);
      usleep(50 * 1000);
    } else {
      struct timespec ts;
      clock_gettime(CLOCK_REALTIME, &ts);
      ts.tv_sec += 1;
      pthread_cond_timedwait(&spawn_cond, &spawn_mutex, &ts);
      pthread_mutex_unlock(&spawn_mutex);
    }
  }

  return NULL;
}

int main(void) {
  Map *mapa = NULL;

  /* Configura o tratamento do sinal Ctrl+C */
  signal(SIGINT, handle_shutdown_signal);

  /* Semeia o gerador de números aleatórios */
  srand(time(NULL));

  /* Executa a inicialização encapsulada */
  if (!init_systems(&mapa)) {
    return EXIT_FAILURE;
  }

  /* Dispara os motores do relógio e dos semáforos */
  clock_start(1000);
  traffic_start();

  // Pool dinâmica de veículos inicialmente vazia
  for (int i = 0; i < MAX_CONCURRENT_VEHICLES; i++) {
    fleet[i] = NULL;
  }

  printf("[MAIN] Ativando a thread de Visualização (Display)...\n");
  display_start(mapa);

  printf("[MAIN] Simulação dinâmica em andamento. Use Ctrl+C para sair.\n");

  SpawnArgs spawn_args = {.mapa = mapa};
  pthread_t spawn_thread;
  if (pthread_create(&spawn_thread, NULL, spawn_manager_routine, &spawn_args) !=
      0) {
    fprintf(stderr, "[MAIN] ERRO: Falha ao criar thread gerenciadora.\n");
    return EXIT_FAILURE;
  }

  while (keep_running == true) {
    sleep(1);
  }

  printf("[MAIN] Encerrando recursos de forma segura...\n");

  // Para os motores para forçar as threads restantes a sair dos seus loops de
  // tick
  clock_stop();
  traffic_stop();
  display_stop();

  pthread_mutex_lock(&spawn_mutex);
  pthread_cond_signal(&spawn_cond);
  pthread_mutex_unlock(&spawn_mutex);
  pthread_join(spawn_thread, NULL);

  printf("[MAIN] Finalizando as threads dos veículos restantes...\n");
  for (int i = 0; i < MAX_CONCURRENT_VEHICLES; i++) {
    if (fleet[i] != NULL) {
      vehicle_destroy(fleet[i]);
      fleet[i] = NULL;
    }
  }

  clock_destroy();
  traffic_destroy();
  if (mapa) {
    destroy(mapa);
  }

  pthread_mutex_destroy(&spawn_mutex);
  pthread_cond_destroy(&spawn_cond);

  printf("[MAIN] Sistema finalizado com sucesso!\n");
  return EXIT_SUCCESS;
}