#include "map.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>


/**
 * Descrição: Arquivo responsável pela lógica de criação da representação do mapa na memória do computador ao ler o arquivo "mapa.txt"
 * Autor: Paulo Gabriel
 */

Map* load_map(const char *path_file) {
    
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
        }
        else {
            current_columns += 1;
        }

    }

    if (current_columns > 0) {
        rows++;
        if (current_columns > columns) {
            columns = current_columns;
        }
    }

    rewind(arquivo);
    Map  *mapa = (Map*) malloc(sizeof(Map));

    mapa ->columns = columns;
    mapa ->rows = rows;
    mapa ->cell_grid = (Cell**) malloc(rows * sizeof(Cell*));

    for (int i = 0; i < rows; i++) {
        mapa ->cell_grid[i] = (Cell*) malloc(columns * sizeof(Cell));

    }

    int i = 0;

    int j = 0;
    
    while ((c = fgetc(arquivo)) !=  EOF) {
        if (c == '\n') {
            i++;
            j = 0;
            continue;
        }
        pthread_mutex_init(&mapa ->cell_grid[i][j].mutex, NULL);

        switch(c) {
            case '>':
                mapa->cell_grid[i][j].direction = 'L';
                mapa ->cell_grid[i][j].type = ROADS;
                break;
            
            case '<':
                mapa ->cell_grid[i][j].direction = 'O';
                mapa->cell_grid[i][j].type = ROADS;
                break;

            case 'v':
                mapa ->cell_grid[i][j].direction = 'S';
                mapa ->cell_grid[i][j].type = ROADS;
                break;

            case '^':
                mapa ->cell_grid[i][j].direction = 'N';
                mapa ->cell_grid[i][j].type = ROADS;
                break;

            case '+':
                mapa->cell_grid[i][j].direction = ' ';
                mapa ->cell_grid[i][j].type = INTERSECTION;
                break;

            case ' ':
                mapa ->cell_grid[i][j].direction = ' ';
                mapa ->cell_grid[i][j].type =EMPTY;
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

    for (int i = 0; i< mapa->rows; i++) {

        for(int j = 0; j< mapa->columns; j++) {
            pthread_mutex_destroy(&mapa->cell_grid[i][j].mutex);
        }

        free(mapa->cell_grid[i]);
    }
    free(mapa ->cell_grid);

    free(mapa);

}