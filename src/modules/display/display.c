#include "display.h"
#include "../clock/clock.h"
#include "../traffic/traffic.h"
#include "../vehicle/vehicle.h" // Aqui podemos incluir para saber os detalhes do veículo
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static pthread_t display_thread;
static Map *shared_map = NULL;
static volatile bool keep_display_running = false;

static void *display_routine(void *arg) {
  (void)arg;

  while (keep_display_running) {
    // 1. Sincroniza com o relógio global
    pthread_mutex_lock(&clock_mutex);
    pthread_cond_wait(&clock_cond, &clock_mutex);
    pthread_mutex_unlock(&clock_mutex);

    if (!keep_display_running)
      break;

    // 2. Limpa a tela (Sequência ANSI mágica)
    printf("\033[H\033[J");

    // Imprime o cabeçalho
    printf("=== SIMULADOR DE TRAFEGO PTHREAD === (Tick: %lu)\n\n", global_tick);

    // 3. Varre e desenha o mapa
    for (int i = 0; i < shared_map->rows; i++) {
      for (int j = 0; j < shared_map->columns; j++) {

        // Trava a célula específica para ler com segurança
        // pthread_mutex_lock(&shared_map->cell_grid[i][j].mutex);

        Cell current = shared_map->cell_grid[i][j];

        if (current.current_vehicle != NULL) {
          // Tem um carro aqui! Imprime uma letra 'C' (ou o ID dele)
          // Para diferenciar ambulância, você pode checar
          // current.current_vehicle->type depois
          printf("\033[1;31mC\033[0m"); // 'C' em vermelho
        } else {
          // Célula vazia: desenha a rua ou espaço vazio
          if (current.type == EMPTY) {
            printf(" ");
          } else if (current.type == INTERSECTION) {
            // Verifica se o sinal está verde para quem anda na horizontal
            // (Leste '>')
            bool is_green_for_horizontal = traffic_is_green(i, j, '>');

            if (is_green_for_horizontal) {
              // Imprime o '+' em VERDE
              printf("\033[32m+\033[0m");
            } else {
              // Imprime o '+' em VERMELHO
              printf("\033[31m+\033[0m");
            }
          } else {
            printf("%c", current.direction); // Imprime a direção (>, <, v, ^)
          }
        }

        // Destrava a célula
        // pthread_mutex_unlock(&shared_map->cell_grid[i][j].mutex);
      }
      printf("\n"); // Quebra de linha da matriz
    }
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
  // Opcional: Dar join na thread aqui se necessário
}