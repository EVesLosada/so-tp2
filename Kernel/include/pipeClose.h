#ifndef PIPE_CLOSE_H
#define PIPE_CLOSE_H

#include <stdint.h>
#include <ioDescriptor.h>
#include <process.h>

/* Cierra un extremo de pipe dentro de una tabla de FDs. */
uint8_t pipeCloseDescriptor(IODescriptor *table, uint64_t fd);

/* Cierra un extremo del proceso actual. */
uint8_t pipeCloseCurrentDescriptor(uint64_t fd);

/* Cierra todos los extremos de pipe de una tabla. */
void pipeCloseAllDescriptors(IODescriptor *table);

/*
 * Cancela la espera por pipe y cierra todos los
 * extremos asociados al PCB de un proceso.
 */
void pipeCleanupProcess(Process *process);

#endif
