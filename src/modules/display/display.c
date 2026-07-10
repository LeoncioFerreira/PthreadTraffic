/**
 * Descrição: Implementação da visualização em console (ASCII) com suporte a
 * duplo buffer para exibição suave do mapa, veículos e semáforos. Autor:
 * Salomão Rodrigues
 */

#include "display.h"
#include "../clock/clock.h"
#include "../traffic/traffic.h"
#include "../vehicle/vehicle.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static pthread_t display_thread;
static Map *shared_map = NULL;
static volatile bool keep_display_running = false;

static void *display_routine(void *arg) {
  (void)arg;

  while (keep_display_running) {
    pthread_mutex_lock(&clock_mutex);
    pthread_cond_wait(&clock_cond, &clock_mutex);
    uint64_t display_tick = global_tick;
    pthread_mutex_unlock(&clock_mutex);

    if (!keep_display_running)
      break;

    /*
     * Pequeno atraso para permitir que os veículos processem o tick
     * antes de o display tirar o snapshot visual.
     */
    usleep(20000); // 20 ms

    char frame_buffer[32768];
    int offset = 0;

    offset += snprintf(frame_buffer + offset, sizeof(frame_buffer) - offset,
                       "\033[H\033[J");

    int rows = shared_map->rows;
    int cols = shared_map->columns;

    size_t total_cells = (size_t)rows * (size_t)cols;
    char *vehicle_snapshot = (char *)malloc(total_cells * sizeof(char));

    if (vehicle_snapshot == NULL) {
      continue;
    }

    int count_cars = 0;
    int count_ambulances = 0;

    pthread_mutex_lock(&map_state_mutex);

    for (int i = 0; i < rows; i++) {
      for (int j = 0; j < cols; j++) {
        Vehicle *v = shared_map->cell_grid[i][j].current_vehicle;

        if (v == NULL) {
          vehicle_snapshot[i * cols + j] = ' ';
        } else if (v->type == AMBULANCE) {
          vehicle_snapshot[i * cols + j] = 'A';
          count_ambulances++;
        } else {
          vehicle_snapshot[i * cols + j] = 'C';
          count_cars++;
        }
      }
    }

    pthread_mutex_unlock(&map_state_mutex);

    offset += snprintf(frame_buffer + offset, sizeof(frame_buffer) - offset,
                       "=== SIMULADOR DE TRAFEGO PTHREAD === (Tick: %lu)\n",
                       display_tick);

    offset += snprintf(frame_buffer + offset, sizeof(frame_buffer) - offset,
                       "Legenda: [\033[1;31mC\033[0m] Carro | "
                       "[\033[5;34mA\033[0m] Ambulancia | "
                       "[\033[32m-\033[0m] Verde Horizontal | "
                       "[\033[32m|\033[0m] Verde Vertical\n");

    offset += snprintf(frame_buffer + offset, sizeof(frame_buffer) - offset,
                       "Veiculos: \033[1;31m%d carro(s)\033[0m | "
                       "\033[5;34m%d ambulancia(s)\033[0m\n",
                       count_cars, count_ambulances);

    // Alerta visual de prioridade da ambulância
    if (traffic_is_priority_active()) {
      offset += snprintf(
          frame_buffer + offset, sizeof(frame_buffer) - offset,
          "\033[1;33m[ALERTA] Prioridade MAXIMA! Ambulancia forcando a "
          "passagem num cruzamento!\033[0m\n");
    } else {
      offset +=
          snprintf(frame_buffer + offset, sizeof(frame_buffer) - offset, "\n");
    }

    // Varre e desenha o mapa usando o snapshot dos veículos
    for (int i = 0; i < rows; i++) {
      for (int j = 0; j < cols; j++) {
        CellType cell_type = shared_map->cell_grid[i][j].type;
        char cell_direction = shared_map->cell_grid[i][j].direction;
        char vehicle_symbol = vehicle_snapshot[i * cols + j];

        if (vehicle_symbol == 'A') {
          offset +=
              snprintf(frame_buffer + offset, sizeof(frame_buffer) - offset,
                       "\033[5;34mA\033[0m");
        } else if (vehicle_symbol == 'C') {
          offset +=
              snprintf(frame_buffer + offset, sizeof(frame_buffer) - offset,
                       "\033[1;31mC\033[0m");
        } else {
          if (cell_type == EMPTY) {
            offset += snprintf(frame_buffer + offset,
                               sizeof(frame_buffer) - offset, " ");
          } else if (cell_type == INTERSECTION) {

            bool is_green_for_horizontal =
                traffic_is_safe_to_enter(i, j, '>', display_tick);

            if (is_green_for_horizontal) {
              // VERDE: horizontal pode passar
              offset +=
                  snprintf(frame_buffer + offset, sizeof(frame_buffer) - offset,
                           "\033[32m-\033[0m");
            } else {
              // VERDE: vertical pode passar
              offset +=
                  snprintf(frame_buffer + offset, sizeof(frame_buffer) - offset,
                           "\033[32m|\033[0m");
            }
          } else {
            offset +=
                snprintf(frame_buffer + offset, sizeof(frame_buffer) - offset,
                         "%c", cell_direction);
          }
        }
      }

      offset +=
          snprintf(frame_buffer + offset, sizeof(frame_buffer) - offset, "\n");
    }

    free(vehicle_snapshot);

    printf("%s", frame_buffer);
    fflush(stdout);
  }

  return NULL;
}

void display_start(Map *map) {
  shared_map = map;
  keep_display_running = true;

  if (pthread_create(&display_thread, NULL, display_routine, NULL) != 0) {
    perror("Erro ao criar thread do display");
    exit(EXIT_FAILURE);
  }
}

void display_stop(void) {
  keep_display_running = false;

  // Como o display depende do relógio, acordamos ele caso esteja dormindo
  pthread_mutex_lock(&clock_mutex);
  pthread_cond_broadcast(&clock_cond);
  pthread_mutex_unlock(&clock_mutex);

  pthread_join(display_thread, NULL);
}

void display_print_legend(void) {
  printf("\n==========================================\n");
  printf("    LEGENDA DA SIMULACAO:\n");
  printf("==========================================\n");
  printf("[\033[1;31mC\033[0m] Carro Civil Normal\n");
  printf("[\033[5;34mA\033[0m] Ambulancia (Tem prioridade!)\n");
  printf("[\033[32m-\033[0m] Semaforo: Verde para via Horizontal\n");
  printf("[\033[32m|\033[0m] Semaforo: Verde para via Vertical\n\n");

  printf("A interface visual ira iniciar em 5 segundos...\n");
  fflush(stdout);
  sleep(5);
}