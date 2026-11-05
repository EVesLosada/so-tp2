#ifndef PIPE_H
#define PIPE_H

#include <stdint.h>
#include <syscall_types.h>

#define MAX_PIPES 32
#define PIPE_BUFFER_SIZE 512
#define PIPE_NAME_MAX_LENGTH 32

typedef struct {
    uint8_t inUse;
    uint64_t id;
    char name[PIPE_NAME_MAX_LENGTH];

    uint8_t buffer[PIPE_BUFFER_SIZE];
    uint64_t readIndex;
    uint64_t writeIndex;
    uint64_t count;

    uint64_t readers;
    uint64_t writers;
} Pipe;

void initializePipeTable(void);

uint64_t pipeCreate(const char *name);
uint64_t pipeOpen(const char *name);

/*
 * Abre un extremo de un pipe existente.
 * mode: PIPE_READ o PIPE_WRITE.
 * Devuelve un descriptor local, o MAX_PROCESS_DESCRIPTORS si falla.
 */
uint64_t pipeOpenDescriptor(const char *name, PipeMode mode);

/*
 * Libera una referencia de lectura o escritura.
 * Devuelve 1 si tuvo exito y 0 si falla.
 * No elimina el pipe ni despierta procesos bloqueados.
 */
uint8_t pipeReleaseEndpoint(uint64_t pipeId, PipeMode mode);




Pipe *findPipeById(uint64_t id);

uint64_t pipeBufferWrite(
    Pipe *pipe,
    const uint8_t *data,
    uint64_t count
);

uint64_t pipeBufferRead(
    Pipe *pipe,
    uint8_t *destination,
    uint64_t count
);

#endif
