#include <stddef.h>
#include <pipeClose.h>
#include <pipe.h>
#include <pipeSync.h>
#include <criticalSection.h>

uint8_t pipeCloseDescriptor(IODescriptor *table, uint64_t fd) {
    if (table == NULL ||
        fd >= MAX_PROCESS_DESCRIPTORS) {
        return 0;
    }

    uint64_t flags = enterCriticalSection();
    IODescriptor descriptor;

    if (!ioGetDescriptor(table, fd, &descriptor) ||
        descriptor.type != IO_DESCRIPTOR_PIPE) {
        leaveCriticalSection(flags);
        return 0;
    }

    PipeMode mode;

    if (descriptor.permissions == IO_ACCESS_READ) {
        mode = PIPE_READ;
    } else if (descriptor.permissions == IO_ACCESS_WRITE) {
        mode = PIPE_WRITE;
    } else {
        leaveCriticalSection(flags);
        return 0;
    }

    Pipe *pipe = findPipeById(descriptor.resourceId);

    if (pipe == NULL ||
        (mode == PIPE_READ && pipe->readers == 0) ||
        (mode == PIPE_WRITE && pipe->writers == 0)) {
        leaveCriticalSection(flags);
        return 0;
    }

    /*
     * Desasociar el FD antes de liberar la referencia.
     * Asi, una segunda llamada no puede cerrar dos veces.
     */
    if (!ioUnmapDescriptor(table, fd)) {
        leaveCriticalSection(flags);
        return 0;
    }

    if (!pipeReleaseEndpoint(descriptor.resourceId, mode)) {
        leaveCriticalSection(flags);
        return 0;
    }

    /*
     * El ultimo escritor permite a los lectores
     * detectar EOF cuando se vacie el buffer.
     */
    if (mode == PIPE_WRITE && pipe->writers == 0) {
        pipeWakeAll(pipe, PIPE_READ);
    }

    /*
     * Sin lectores, los escritores bloqueados
     * no tienen motivo para continuar esperando.
     */
    if (mode == PIPE_READ && pipe->readers == 0) {
        pipeWakeAll(pipe, PIPE_WRITE);
    }

    leaveCriticalSection(flags);
    return 1;
}

uint8_t pipeCloseCurrentDescriptor(uint64_t fd) {
    return pipeCloseDescriptor(
        ioGetCurrentDescriptorTable(),
        fd
    );
}

void pipeCloseAllDescriptors(IODescriptor *table) {
    if (table == NULL) {
        return;
    }

    for (uint64_t fd = 0; fd < MAX_PROCESS_DESCRIPTORS; fd++) {
        pipeCloseDescriptor(table, fd);
    }
}


void pipeCleanupProcess(Process *process) {
    if (process == NULL) {
        return;
    }

    uint64_t flags = enterCriticalSection();

    /*
     * Un proceso terminado no debe permanecer
     * en una cola de espera.
     */
    if (process->waitingPipeId != 0) {
        Pipe *pipe = findPipeById(process->waitingPipeId);

        if (pipe != NULL) {
            pipeRemoveWaiter(
                pipe,
                process->waitingPipeMode,
                process->pid
            );
        }

        process->waitingPipeId = 0;
        process->waitingPipeMode = PIPE_READ;
    }

    /*
     * El cierre desasocia cada descriptor, por lo que
     * repetir esta limpieza no decrementa dos veces
     * las referencias.
     */
    pipeCloseAllDescriptors(process->ioDescriptors);

    leaveCriticalSection(flags);
}
