#include <stddef.h>
#include <semaphore.h>
#include <criticalSection.h>

/*
 * Tabla estatica de semaforos.
 * No depende del Memory Manager.
 */
static Semaphore semaphoreTable[MAX_SEMAPHORES];

void initializeSemaphoreTable(void) {
    uint64_t flags = enterCriticalSection();

    for (uint64_t i = 0; i < MAX_SEMAPHORES; i++) {
        Semaphore *sem = &semaphoreTable[i];

        sem->inUse = 0;
        sem->id = 0;
        sem->value = 0;
        sem->openCount = 0;
        sem->waitingHead = 0;
        sem->waitingTail = 0;
        sem->waitingCount = 0;

        for (uint64_t j = 0;
             j < SEMAPHORE_NAME_MAX_LENGTH;
             j++) {
            sem->name[j] = '\0';
        }

        for (uint64_t j = 0;
             j < SEMAPHORE_MAX_WAITERS;
             j++) {
            sem->waitingPids[j] = 0;
        }
    }

    leaveCriticalSection(flags);
}

/*
 * Obtiene una posicion de la tabla.
 *
 * Retorna NULL si el indice esta fuera de rango.
 * La posicion puede estar libre (inUse == 0).
 *
 * Quien consulte o modifique varios campos del slot
 * debe mantener una seccion critica durante toda
 * la operacion.
 */
Semaphore *getSemaphoreSlot(uint64_t index) {
    if (index >= MAX_SEMAPHORES) {
        return NULL;
    }

    return &semaphoreTable[index];
}

/*
 * Agrega un PID al final de la cola FIFO.
 *
 * Retorna 1 si se agrego.
 * Retorna 0 si el semaforo no es valido,
 * la cola esta llena o el PID ya esta en ella.
 */
uint8_t enqueueSemaphoreWaiter(
    Semaphore *sem,
    uint64_t pid
) {
    if (sem == NULL || pid == 0) {
        return 0;
    }

    uint64_t flags = enterCriticalSection();

    if (!sem->inUse ||
        sem->waitingCount >= SEMAPHORE_MAX_WAITERS) {
        leaveCriticalSection(flags);
        return 0;
    }

    /* Impedir duplicados. */
    for (uint64_t i = 0; i < sem->waitingCount; i++) {
        uint64_t index =
            (sem->waitingHead + i) % SEMAPHORE_MAX_WAITERS;

        if (sem->waitingPids[index] == pid) {
            leaveCriticalSection(flags);
            return 0;
        }
    }

    sem->waitingPids[sem->waitingTail] = pid;

    sem->waitingTail =
        (sem->waitingTail + 1) % SEMAPHORE_MAX_WAITERS;

    sem->waitingCount++;

    leaveCriticalSection(flags);
    return 1;
}

/*
 * Retira el primer PID de la cola FIFO.
 *
 * Retorna 0 si la cola esta vacia o el
 * semaforo no es valido.
 */
uint64_t dequeueSemaphoreWaiter(Semaphore *sem) {
    if (sem == NULL) {
        return 0;
    }

    uint64_t flags = enterCriticalSection();

    if (!sem->inUse || sem->waitingCount == 0) {
        leaveCriticalSection(flags);
        return 0;
    }

    uint64_t pid = sem->waitingPids[sem->waitingHead];

    sem->waitingPids[sem->waitingHead] = 0;

    sem->waitingHead =
        (sem->waitingHead + 1) % SEMAPHORE_MAX_WAITERS;

    sem->waitingCount--;

    leaveCriticalSection(flags);
    return pid;
}
