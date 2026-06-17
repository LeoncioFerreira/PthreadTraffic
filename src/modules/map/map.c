#include "map.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Descrição: Arquivo responsável pela lógica de criação da representação do
 * mapa na memória do computador ao ler o arquivo "mapa.txt" Autor: Paulo
 * Gabriel
 */

Map *load_map(const char *path_file) {

  FILE *arquivo = fopen(path_file, "r");

  if (arquivo == NULL) {
    perror("Erro ao abrir o arquivo");
    return NULL;
  }

  int rows = 0;
  int columns = 0;
  int current_columns = 0;
  int c;

  while ((c = fgetc(arquivo)) != EOF) {
    if (c == '\n') {
      rows += 1;

      if (current_columns > columns) {
        columns = current_columns;
      }
      current_columns = 0;

    } else if (c == '\r') {

      continue;
    } else {
      current_columns += 1;
    }
  }

  if (current_columns > 0) {
    rows++;
    if (current_columns > columns) {
      columns = current_columns;
    }
  }

  if (rows == 0 || columns == 0) {
    fprintf(stderr, "Erro: arquivo de mapa vazio ou sem conteúdo válido (%s)\n",
            path_file);
    fclose(arquivo);
    return NULL;
  }

  rewind(arquivo);
  Map *mapa = (Map *)malloc(sizeof(Map));

  if (mapa == NULL) {
    perror("Erro fatal: sem memória para a estrutura Map");
    fclose(arquivo);
    return NULL;
  }

  mapa->columns = columns;
  mapa->rows = rows;
  mapa->cell_grid = (Cell **)malloc(rows * sizeof(Cell *));

  if (mapa->cell_grid == NULL) {
    perror("Erro: sem memória para a matriz de células");
    free(mapa);
    fclose(arquivo);
    return NULL;
  }

  for (int i = 0; i < rows; i++) {
    mapa->cell_grid[i] = (Cell *)malloc(columns * sizeof(Cell));

    if (mapa->cell_grid[i] == NULL) {
      perror("Erro fatal: sem memória para as colunas");

      for (int k = 0; k < i; k++) {

        for (int c_idx = 0; c_idx < columns; c_idx++) {
          pthread_mutex_destroy(&mapa->cell_grid[k][c_idx].mutex);
        }

        free(mapa->cell_grid[k]);
      }
      free(mapa->cell_grid);
      free(mapa);
      fclose(arquivo);
      return NULL;
    }
    for (int j = 0; j < columns; j++) {
      mapa->cell_grid[i][j].type = EMPTY;
      mapa->cell_grid[i][j].direction = ' ';
      pthread_mutex_init(&mapa->cell_grid[i][j].mutex, NULL);
    }
  }

  int i = 0;

  int j = 0;

  while ((c = fgetc(arquivo)) != EOF) {
    if (c == '\n') {
      i++;
      j = 0;
      continue;
    }

    if (c == '\r') {
      continue;
    }

    if (i >= rows || j >= columns) {
      continue;
    }

    switch (c) {
    case '>':
      mapa->cell_grid[i][j].direction = 'L';
      mapa->cell_grid[i][j].type = ROADS;
      break;

    case '<':
      mapa->cell_grid[i][j].direction = 'O';
      mapa->cell_grid[i][j].type = ROADS;
      break;

    case 'v':
      mapa->cell_grid[i][j].direction = 'S';
      mapa->cell_grid[i][j].type = ROADS;
      break;

    case '^':
      mapa->cell_grid[i][j].direction = 'N';
      mapa->cell_grid[i][j].type = ROADS;
      break;

    case '+':
      mapa->cell_grid[i][j].direction = ' ';
      mapa->cell_grid[i][j].type = INTERSECTION;
      break;

    case ' ':
      mapa->cell_grid[i][j].direction = ' ';
      mapa->cell_grid[i][j].type = EMPTY;
      break;

    default:
      fprintf(stderr,
              "Aviso: caractere desconhecido '%c' (0x%02X) em (%d,%d), "
              "tratado como célula vazia\n",
              c, c, i, j);
      mapa->cell_grid[i][j].direction = ' ';
      mapa->cell_grid[i][j].type = EMPTY;
      break;
    }
    j++;
  }

  fclose(arquivo);
  return mapa;
}

void destroy(Map *mapa) {
  if (mapa == NULL) {
    return;
  }

  for (int i = 0; i < mapa->rows; i++) {

    for (int j = 0; j < mapa->columns; j++) {
      pthread_mutex_destroy(&mapa->cell_grid[i][j].mutex);
    }

    free(mapa->cell_grid[i]);
  }
  free(mapa->cell_grid);

  free(mapa);
}