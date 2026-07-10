/**
 * Descrição: Definição do módulo de logging concorrente (thread-safe). Provede
 * funções para gravação de eventos e alertas formatados no arquivo
 * simulador.log. Autor: André
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdbool.h>

/* Níveis de severidade do log */
typedef enum { LOG_INFO, LOG_ALERT } LogLevel;

/**
 * Inicializa o subsistema de log abrindo o arquivo simulador.log e
 * configurando o mutex de exclusão mútua interno.
 * Retorna true se houver sucesso, false caso contrário.
 */
bool logger_init(const char *filename);

/**
 * Grava uma mensagem formatada de forma thread-safe associando o tick
 * atual capturado a partir do módulo clock.
 */
void logger_write(LogLevel level, const char *format, ...);

/**
 * Fecha o descritor de arquivo de log de forma segura e destrói o mutex.
 */
void logger_destroy(void);

#endif /* LOGGER_H */