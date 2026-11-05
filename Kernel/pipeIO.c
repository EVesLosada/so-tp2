#include <stddef.h>
#include <pipeIO.h>
#include <pipe.h>
#include <pipeSync.h>
#include <processManager.h>
#include <criticalSection.h>

uint64_t pipeReadBlocking(
    uint64_t pipeId,
    uint8_t *destination,
    uint64_t count
) {
    if (pipeId == 0 || destination == NULL || count == 0) {
        return 0;
    }

    for (;;) {
        uint64_t flags = enterCriticalSection();
        Pipe *pipe = findPipeById(pipeId);

        if (pipe == NULL) {
            leaveCriticalSection(flags);
            return 0;
        }

        /*
         * Si hay datos, leerlos antes de comprobar EOF.
         * Asi no perdemos datos escritos previamente.
         */
        if (pipe->count > 0) {
            uint64_t bytesRead =
                pipeBufferRead(pipe, destination, count);

            if (bytesRead > 0) {
                /* Hay espacio para algun escritor bloqueado. */
                pipeWakeOne(pipe, PIPE_WRITE);
            }

            leaveCriticalSection(flags);
            return bytesRead;
        }

        /*
         * Buffer vacio y ningun escritor:
         * no llegaran mas datos; devolver EOF.
         */
        if (pipe->writers == 0) {
            leaveCriticalSection(flags);
            return 0;
        }

        Process *current = getCurrentProcess();

        /*
         * La shell sin PCB y los contextos invalidos
         * no pueden bloquearse con este mecanismo.
         */
        if (current == NULL ||
            current->pid == 0 ||
            current->state != PROCESS_RUNNING ||
            current->waitingSemaphoreId != 0 ||
            current->waitingPipeId != 0) {
            leaveCriticalSection(flags);
            return 0;
        }

        /*
         * La comprobacion del buffer, el encolado
         * y el bloqueo ocurren bajo la misma
         * seccion critica.
         */
        if (!pipeEnqueueWaiter(
                pipe, PIPE_READ, current->pid)) {
            leaveCriticalSection(flags);
            return 0;
        }

        current->waitingPipeId = pipeId;
        current->waitingPipeMode = PIPE_READ;

        if (!blockProcess(current->pid)) {
            pipeRemoveWaiter(pipe, PIPE_READ, current->pid);

            current->waitingPipeId = 0;
            current->waitingPipeMode = PIPE_READ;

            leaveCriticalSection(flags);
            return 0;
        }

        /*
         * Al reanudarnos, restauramos las interrupciones
         * y volvemos a comprobar los datos y el EOF.
         */
        leaveCriticalSection(flags);
    }
}

/*
 * Escritura bloqueante en un pipe.
 *
 * Escribe mientras haya espacio. Si el buffer se llena,
 * espera a que un lector consuma datos.
 *
 * Si desaparecen todos los lectores, retorna los bytes
 * que haya conseguido escribir hasta ese momento.
 */
uint64_t pipeWriteBlocking(
    uint64_t pipeId,
    const uint8_t *source,
    uint64_t count
) {
    if (pipeId == 0 || source == NULL || count == 0) {
        return 0;
    }

    uint64_t totalWritten = 0;

    while (totalWritten < count) {
        uint64_t flags = enterCriticalSection();
        Pipe *pipe = findPipeById(pipeId);

        if (pipe == NULL) {
            leaveCriticalSection(flags);
            return totalWritten;
        }

        /*
         * No tiene sentido seguir escribiendo si
         * ya no existe ningun extremo lector.
         */
        if (pipe->readers == 0) {
            leaveCriticalSection(flags);
            return totalWritten;
        }

        /*
         * Hay espacio: escribir todos los bytes
         * posibles y avisar a un lector.
         */
        if (pipe->count < PIPE_BUFFER_SIZE) {
            uint64_t written = pipeBufferWrite(
                pipe,
                source + totalWritten,
                count - totalWritten
            );

            totalWritten += written;

            if (written > 0) {
                pipeWakeOne(pipe, PIPE_READ);
            }

            leaveCriticalSection(flags);
            continue;
        }

        /*
         * Buffer lleno: necesitamos un proceso
         * real en ejecucion para poder bloquear.
         */
        Process *current = getCurrentProcess();

        if (current == NULL ||
            current->pid == 0 ||
            current->state != PROCESS_RUNNING ||
            current->waitingSemaphoreId != 0 ||
            current->waitingPipeId != 0) {
            leaveCriticalSection(flags);
            return totalWritten;
        }

        /*
         * Registrar la espera antes de bloquear,
         * manteniendo la misma seccion critica.
         */
        if (!pipeEnqueueWaiter(
                pipe, PIPE_WRITE, current->pid)) {
            leaveCriticalSection(flags);
            return totalWritten;
        }

        current->waitingPipeId = pipeId;
        current->waitingPipeMode = PIPE_WRITE;

        if (!blockProcess(current->pid)) {
            pipeRemoveWaiter(pipe, PIPE_WRITE, current->pid);

            current->waitingPipeId = 0;
            current->waitingPipeMode = PIPE_READ;

            leaveCriticalSection(flags);
            return totalWritten;
        }

        /*
         * Al despertar, volver al principio:
         * otro escritor pudo ocupar el espacio.
         */
        leaveCriticalSection(flags);
    }

    return totalWritten;
}
