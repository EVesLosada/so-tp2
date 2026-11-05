#ifndef PIPE_SYNC_H
#define PIPE_SYNC_H

#include <stdint.h>
#include <pipe.h>

/* Despierta al primer proceso valido de la cola. */
uint8_t pipeWakeOne(Pipe *pipe, PipeMode mode);

/* Despierta a todos los procesos validos de la cola. */
uint64_t pipeWakeAll(Pipe *pipe, PipeMode mode);

#endif
