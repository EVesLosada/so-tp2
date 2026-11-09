#ifndef MVAR_H
#define MVAR_H

#include <stdint.h>

typedef struct {
    uint64_t emptySem;
    uint64_t fullSem;
    uint64_t value;
    uint8_t initialized;
} MVar;

/*
 * Inicializa un MVar vacio.
 * Los nombres de los semaforos deben ser diferentes.
 */
uint8_t mvarInit(
    MVar *mvar,
    const char *emptyName,
    const char *fullName
);

/* Bloquea hasta poder escribir un valor. */
uint8_t mvarPut(MVar *mvar, uint64_t value);

/* Bloquea hasta poder retirar un valor. */
uint8_t mvarTake(MVar *mvar, uint64_t *value);

/* Cierra los semaforos del MVar. */
uint8_t mvarClose(MVar *mvar);

#endif
