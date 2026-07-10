/**
 * Descrição: Implementação do spawner concorrente que insere novos carros na
 * malha de acordo com regras de ocupação e limites definidos. Autor: Salomão
 * Rodrigues
 */

#define _GNU_SOURCE
#include "spawner.h"
#include "../vehicle/vehicle.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define NUM_SPAWN_POINTS 12

static const int spawn_positions[NUM_SPAWN_POINTS][2] = {
    {3, 0},  {4, 42}, {8, 0},   {9, 42}, {13, 0},  {14, 42},
    {17, 7}, {0, 8},  {17, 16}, {0, 17}, {17, 25}, {0, 26}};

static pthread_mutex_t spawn_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t spawn_cond = PTHREAD_COND_INITIALIZER;
static int veiculos_ativos = 0;
static int ambulancias_ativas = 0;
static bool ambulance_spawned = false;
static time_t last_ambulance_exit_time = 0;

static Vehicle **fleet = NULL;
static int limit_vehicles = 0;
static unsigned int vehicle_id_counter = 1;
static pthread_t spawn_thread;
static volatile bool keep_spawner_running = false;

static bool try_spawn_one(Map *mapa) {
  VehicleType type;
  int speed_ticks;
  time_t now = time(NULL);

  if (!ambulance_spawned ||
      (ambulancias_ativas == 0 && now - last_ambulance_exit_time >= 15)) {
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
      for (int s = 0; s < limit_vehicles; s++) {
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

static void *spawn_manager_routine(void *arg) {
  Map *mapa = (Map *)arg;

  while (keep_spawner_running) {
    pthread_mutex_lock(&spawn_mutex);

    // Garbage Collection de Threads: percorre a pool procurando por veículos
    // que saíram do mapa para libertar recursos
    for (int i = 0; i < limit_vehicles; i++) {
      if (fleet[i] != NULL) {
        if (pthread_tryjoin_np(fleet[i]->thread_id, NULL) == 0) {
          if (fleet[i]->type == AMBULANCE) {
            ambulancias_ativas--;
            last_ambulance_exit_time = time(NULL);
          }
          free(fleet[i]);
          fleet[i] = NULL;
          veiculos_ativos--;
        }
      }
    }

    // PASSO 2: Spawner Automático Dinâmico
    while (veiculos_ativos < limit_vehicles && keep_spawner_running) {
      if (try_spawn_one(mapa)) {
        veiculos_ativos++;
      } else {
        break;
      }
    }

    if (veiculos_ativos < limit_vehicles) {
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

bool spawner_start(int max_vehicles, Map *mapa) {
  limit_vehicles = max_vehicles;
  fleet = (Vehicle **)calloc(limit_vehicles, sizeof(Vehicle *));
  if (fleet == NULL)
    return false;

  keep_spawner_running = true;

  if (pthread_create(&spawn_thread, NULL, spawn_manager_routine, mapa) != 0) {
    free(fleet);
    fleet = NULL;
    return false;
  }
  return true;
}

void spawner_stop(void) {
  keep_spawner_running = false;

  pthread_mutex_lock(&spawn_mutex);
  pthread_cond_signal(&spawn_cond);
  pthread_mutex_unlock(&spawn_mutex);

  pthread_join(spawn_thread, NULL);
}

void spawner_cleanup(void) {
  if (fleet == NULL)
    return;

  for (int i = 0; i < limit_vehicles; i++) {
    if (fleet[i] != NULL) {
      vehicle_destroy(fleet[i]);
      fleet[i] = NULL;
    }
  }

  free(fleet);
  fleet = NULL;
}
