#include <stdint.h>
#include <stddef.h>

#include <semaphore.h>
#include <processManager.h>
#include <criticalSection.h>

/*
 * Publica una unidad del semaforo.
 *
 * Si hay procesos esperando, despierta al primero
 * que siga bloqueado, respetando el orden FIFO.
 *
 * Si no quedan procesos esperando, incrementa
 * el contador.
 *
 * Retorna 1 si tuvo exito y 0 si falla.
 */
uint8_t semPost(uint64_t id) {
    if (id == 0) {
        return 0;
    }

    uint64_t flags = enterCriticalSection();

    for (uint64_t i = 0; i < MAX_SEMAPHORES; i++) {
        Semaphore *sem = getSemaphoreSlot(i);

        if (!sem->inUse || sem->id != id) {
            continue;
        }

        /*
         * Transferir el recurso a un proceso esperando,
         * sin aumentar el contador.
         */
        while (sem->waitingCount > 0) {
            uint64_t pid = dequeueSemaphoreWaiter(sem);

            /*
             * Si el proceso ya no esta bloqueado
             * (por ejemplo, porque termino), probar
             * con el siguiente.
             */
            if (pid != 0 && unblockProcessFromSemaphore(pid, id)) {
                leaveCriticalSection(flags);
                return 1;
            }
        }

        /* Evitar overflow del contador. */
        if (sem->value == UINT64_MAX) {
            leaveCriticalSection(flags);
            return 0;
        }

        sem->value++;

        leaveCriticalSection(flags);
        return 1;
    }

    leaveCriticalSection(flags);
    return 0;
}

/*
 * Espera una unidad del semaforo.
 *
 * Si hay recursos disponibles, consume uno.
 * Si no hay, encola el PID y bloquea el proceso.
 *
 * El bloqueo se hace dentro de la seccion critica
 * para evitar perder un semPost entre ambas acciones.
 */
uint8_t semWait(uint64_t id) {
    if (id == 0) {
        return 0;
    }

    uint64_t flags = enterCriticalSection();

    for (uint64_t i = 0; i < MAX_SEMAPHORES; i++) {
        Semaphore *sem = getSemaphoreSlot(i);

        if (sem == NULL || !sem->inUse || sem->id != id) {
            continue;
        }

        /* Hay un recurso disponible: consumirlo. */
        if (sem->value > 0) {
            sem->value--;
            leaveCriticalSection(flags);
            return 1;
        }

        /* Sin recursos, necesitamos un proceso ejecutandose. */
        Process *current = getCurrentProcess();

        if (current == NULL ||
            current->pid == 0 ||
            current->state != PROCESS_RUNNING) {
            leaveCriticalSection(flags);
            return 0;
        }

        /* Registrarlo antes de bloquearlo. */
        if (!enqueueSemaphoreWaiter(sem, current->pid)) {
            leaveCriticalSection(flags);
            return 0;
        }

        current->waitingSemaphoreId = id;

        /*
         * En el kernel real, blockProcess realiza
         * el cambio de contexto y retorna cuando
         * el proceso es reanudado.
         */
        if (!blockProcess(current->pid)) {
            /*
             * El encolado fue el ultimo de la cola
             * y no hubo cambios concurrentes porque
             * mantenemos las interrupciones deshabilitadas.
             */
            sem->waitingTail =
                (sem->waitingTail + SEMAPHORE_MAX_WAITERS - 1)
                % SEMAPHORE_MAX_WAITERS;

            sem->waitingPids[sem->waitingTail] = 0;
            sem->waitingCount--;
            current->waitingSemaphoreId = 0;

            leaveCriticalSection(flags);
            return 0;
        }

        /*
         * semPost le transfirio el recurso al
         * proceso despertado, por lo que no
         * decrementamos el contador nuevamente.
         */
        leaveCriticalSection(flags);
        return 1;
    }

    leaveCriticalSection(flags);
    return 0;
}
