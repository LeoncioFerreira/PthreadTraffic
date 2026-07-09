#include "display.h"
#include "../clock/clock.h"
#include "../traffic/traffic.h"
#include "../vehicle/vehicle.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

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

    char frame_buffer[32768];
    int offset = 0;

    offset += snprintf(frame_buffer + offset, sizeof(frame_buffer) - offset,
                       "\033[H\033[J");

    int count_cars = 0;
    int count_ambulances = 0;
    for (int i = 0; i < shared_map->rows; i++) {
      for (int j = 0; j < shared_map->columns; j++) {
        Vehicle *v = shared_map->cell_grid[i][j].current_vehicle;
        if (v != NULL) {
          if (v->type == AMBULANCE)
            count_ambulances++;
          else
            count_cars++;
        }
      }
    }

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

    // Varre e desenha o mapa
    for (int i = 0; i < shared_map->rows; i++) {
      for (int j = 0; j < shared_map->columns; j++) {
        CellType cell_type;
        char cell_direction;
        Vehicle *current_vehicle;

        if (pthread_mutex_trylock(&shared_map->cell_grid[i][j].mutex) == 0) {
          cell_type = shared_map->cell_grid[i][j].type;
          cell_direction = shared_map->cell_grid[i][j].direction;
          current_vehicle = shared_map->cell_grid[i][j].current_vehicle;
          pthread_mutex_unlock(&shared_map->cell_grid[i][j].mutex);
        } else {
          cell_type = shared_map->cell_grid[i][j].type;
          cell_direction = shared_map->cell_grid[i][j].direction;
          current_vehicle = shared_map->cell_grid[i][j].current_vehicle;
        }

        if (current_vehicle != NULL) {
          if (current_vehicle->type == AMBULANCE) {
            offset +=
                snprintf(frame_buffer + offset, sizeof(frame_buffer) - offset,
                         "\033[5;34mA\033[0m");
          } else {
            // Carro normal (vermelho)
            offset +=
                snprintf(frame_buffer + offset, sizeof(frame_buffer) - offset,
                         "\033[1;31mC\033[0m");
          }
        } else {
          if (cell_type == EMPTY) {
            offset += snprintf(frame_buffer + offset,
                               sizeof(frame_buffer) - offset, " ");
          } else if (cell_type == INTERSECTION) {
            // Garante consistência com o que os veículos viram neste tick.
            bool is_green_for_horizontal =
                traffic_is_safe_to_enter(i, j, '>', display_tick);
            if (is_green_for_horizontal) {
              // VERDE: horizontal (← →) pode passar
              offset +=
                  snprintf(frame_buffer + offset, sizeof(frame_buffer) - offset,
                           "\033[32m-\033[0m");
            } else {
              // VERMELHO: vertical (↑ ↓) pode passar, horizontal fechado
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
    printf("%s", frame_buffer);
    fflush(stdout); // Força a atualização da tela imediatamente
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
