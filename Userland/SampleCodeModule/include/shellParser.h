#ifndef SHELL_PARSER_H
#define SHELL_PARSER_H

#include <stdint.h>

#define SHELL_MAX_LINE_LENGTH 64
#define SHELL_MAX_ARGUMENTS 16

typedef struct {
    char buffer[SHELL_MAX_LINE_LENGTH];
    char *argv[SHELL_MAX_ARGUMENTS];
    uint64_t argc;

    /* Segundo comando de un pipeline, si existe. */
    char *pipeArgv[SHELL_MAX_ARGUMENTS];
    uint64_t pipeArgc;

    uint8_t background;
} ShellParsedCommand;

/*
 * Separa nombre del comando, argumentos y '&'.
 *
 * Retorna 1 si la entrada es valida.
 * Retorna 0 si la sintaxis es invalida.
 *
 * argc incluye el nombre del programa en argv[0].
 */
uint8_t shellParseCommand(
    const char *line,
    ShellParsedCommand *result
);

#endif
