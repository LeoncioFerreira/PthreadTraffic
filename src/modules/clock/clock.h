/**
 * Descrição: Definição do módulo de relógio responsável por gerenciar o tempo
 * global da simulação e orquestrar a sincronização temporal entre as threads.
 */

#ifndef CLOCK_H
#define CLOCK_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

extern uint64_t global_tick;
extern pthread_mutex_t clock_mutex;
extern pthread_cond_t clock_cond;

/*
 * Inicializa os recursos de sincronização do relógio (mutex e variável de
 * condição).
 */
void clock_init(void);

/*
 * Inicia a thread responsável pelo relógio.
 */
void clock_start(int interval_ms);

/*
 * Sinaliza para a thread do relógio parar e aguarda sua finalização (join).
 */
void clock_stop(void);

/*
 * Destrói os recursos de sincronização (mutex e variável de condição).
 */
void clock_destroy(void);

#endif /* CLOCK_H */