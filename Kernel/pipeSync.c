#include <stddef.h>
#include <pipeSync.h>
#include <processManager.h>
#include <criticalSection.h>

/*
 * Selecciona una cola de espera valida.
 * Debe utilizarse dentro de una seccion critica.
 */
static PipeWaitQueue *getWaitQueue(Pipe *pipe, PipeMode mode) {
    if (pipe == NULL || !pipe->inUse || pipe->id == 0) {
        return NULL;
    }

    if (mode == PIPE_READ) {
        return &pipe->readerWaiters;
    }

    if (mode == PIPE_WRITE) {
        return &pipe->writerWaiters;
    }

    return NULL;
}

/*
 * Despierta al primer proceso que siga esperando
 * este pipe y este modo.
 *
 * Descarta PIDs obsoletos.
 */
uint8_t pipeWakeOne(Pipe *pipe, PipeMode mode) {
    uint64_t flags = enterCriticalSection();
    PipeWaitQueue *queue = getWaitQueue(pipe, mode);

    if (queue == NULL) {
        leaveCriticalSection(flags);
        return 0;
    }

    while (queue->count > 0) {
        uint64_t pid = pipeDequeueWaiter(pipe, mode);

        if (pid == 0) {
            break;
        }

        if (unblockProcessFromPipe(pid, pipe->id, mode)) {
            leaveCriticalSection(flags);
            return 1;
        }
    }

    leaveCriticalSection(flags);
    return 0;
}

/*
 * Despierta a todos los procesos validos que
 * esperan en una de las colas del pipe.
 */
uint64_t pipeWakeAll(Pipe *pipe, PipeMode mode) {
    uint64_t flags = enterCriticalSection();
    PipeWaitQueue *queue = getWaitQueue(pipe, mode);

    if (queue == NULL) {
        leaveCriticalSection(flags);
        return 0;
    }

    uint64_t awakened = 0;

    while (queue->count > 0) {
        uint64_t pid = pipeDequeueWaiter(pipe, mode);

        if (pid == 0) {
            break;
        }

        if (unblockProcessFromPipe(pid, pipe->id, mode)) {
            awakened++;
        }
    }

    leaveCriticalSection(flags);
    return awakened;
}
