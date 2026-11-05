#ifndef PIPE_IO_H
#define PIPE_IO_H

#include <stdint.h>

/*
 * Lee datos de un pipe identificado por su ID.
 * Devuelve los bytes leidos, o 0 ante EOF o fallo.
 *
 * Debe invocarse desde el contexto del proceso lector
 * si puede ser necesario bloquear.
 */
uint64_t pipeReadBlocking(
    uint64_t pipeId,
    uint8_t *destination,
    uint64_t count
);


/*
 * Escribe datos en un pipe.
 * Si el buffer esta lleno, espera espacio disponible.
 * Devuelve la cantidad de bytes escritos.
 */
uint64_t pipeWriteBlocking(
    uint64_t pipeId,
    const uint8_t *source,
    uint64_t count
);

#endif
