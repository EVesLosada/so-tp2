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

/*
 * Proximo identificador disponible.
 * El ID 0 se reserva para indicar error.
 */
static uint64_t nextSemaphoreId = 1;

/*
 * Compara dos nombres validos.
 */
static uint8_t semaphoreNamesEqual(
    const char *first,
    const char *second
) {
    for (uint64_t i = 0; i < SEMAPHORE_NAME_MAX_LENGTH; i++) {
        if (first[i] != second[i]) {
            return 0;
        }

        if (first[i] == '\0') {
            return 1;
        }
    }

    return 0;
}

/*
 * Comprueba que el nombre no este vacio y que
 * termine antes de alcanzar el limite.
 */
static uint8_t validSemaphoreName(const char *name) {
    if (name == NULL) {
        return 0;
    }

    for (uint64_t i = 0; i < SEMAPHORE_NAME_MAX_LENGTH; i++) {
        if (name[i] == '\0') {
            return i > 0;
        }
    }

    return 0;
}

/*
 * Crea un semaforo nuevo.
 */
uint64_t semCreate(const char *name, uint64_t initialValue) {
    if (!validSemaphoreName(name)) {
        return 0;
    }

    uint64_t flags = enterCriticalSection();
    Semaphore *freeSlot = NULL;

    for (uint64_t i = 0; i < MAX_SEMAPHORES; i++) {
        Semaphore *sem = &semaphoreTable[i];

        if (sem->inUse) {
            if (semaphoreNamesEqual(sem->name, name)) {
                leaveCriticalSection(flags);
                return 0;
            }
        } else if (freeSlot == NULL) {
            freeSlot = sem;
        }
    }

    if (freeSlot == NULL || nextSemaphoreId == 0) {
        leaveCriticalSection(flags);
        return 0;
    }

    freeSlot->inUse = 1;
    freeSlot->id = nextSemaphoreId++;
    freeSlot->value = initialValue;
    freeSlot->openCount = 1;
    freeSlot->waitingHead = 0;
    freeSlot->waitingTail = 0;
    freeSlot->waitingCount = 0;

    for (uint64_t i = 0; i < SEMAPHORE_NAME_MAX_LENGTH; i++) {
        freeSlot->name[i] = name[i];

        if (name[i] == '\0') {
            break;
        }
    }

    for (uint64_t i = 0; i < SEMAPHORE_MAX_WAITERS; i++) {
        freeSlot->waitingPids[i] = 0;
    }

    uint64_t id = freeSlot->id;
    leaveCriticalSection(flags);
    return id;
}

/*
 * Abre un semaforo existente por nombre.
 */
uint64_t semOpen(const char *name) {
    if (!validSemaphoreName(name)) {
        return 0;
    }

    uint64_t flags = enterCriticalSection();

    for (uint64_t i = 0; i < MAX_SEMAPHORES; i++) {
        Semaphore *sem = &semaphoreTable[i];

        if (sem->inUse &&
            semaphoreNamesEqual(sem->name, name)) {

            if (sem->openCount == UINT64_MAX) {
                leaveCriticalSection(flags);
                return 0;
            }

            sem->openCount++;
            uint64_t id = sem->id;

            leaveCriticalSection(flags);
            return id;
        }
    }

    leaveCriticalSection(flags);
    return 0;
}

/*
 * Cierra una apertura del semaforo.
 */
uint8_t semClose(uint64_t id) {
    if (id == 0) {
        return 0;
    }

    uint64_t flags = enterCriticalSection();

    for (uint64_t i = 0; i < MAX_SEMAPHORES; i++) {
        Semaphore *sem = &semaphoreTable[i];

        if (!sem->inUse || sem->id != id) {
            continue;
        }

        if (sem->openCount == 0) {
            leaveCriticalSection(flags);
            return 0;
        }

        /*
         * No eliminar el semaforo si todavia
         * hay procesos esperando.
         */
        if (sem->openCount == 1 && sem->waitingCount > 0) {
            leaveCriticalSection(flags);
            return 0;
        }

        sem->openCount--;

        if (sem->openCount == 0) {
            sem->inUse = 0;
            sem->id = 0;
            sem->value = 0;
            sem->waitingHead = 0;
            sem->waitingTail = 0;
            sem->waitingCount = 0;

            for (uint64_t j = 0;
                 j < SEMAPHORE_NAME_MAX_LENGTH; j++) {
                sem->name[j] = '\0';
            }

            for (uint64_t j = 0;
                 j < SEMAPHORE_MAX_WAITERS; j++) {
                sem->waitingPids[j] = 0;
            }
        }

        leaveCriticalSection(flags);
        return 1;
    }

    leaveCriticalSection(flags);
    return 0;
}

/*
 * Elimina un PID terminado de todas las colas de espera.
 * Conserva el orden FIFO de los demas procesos.
 *
 * Retorna 1 si elimino alguna espera; 0 en caso contrario.
 */
uint8_t semCancelWaiter(uint64_t pid) {
    if (pid == 0) {
        return 0;
    }

    uint64_t flags = enterCriticalSection();
    uint8_t removed = 0;

    for (uint64_t i = 0; i < MAX_SEMAPHORES; i++) {
        Semaphore *sem = &semaphoreTable[i];

        if (!sem->inUse || sem->waitingCount == 0) {
            continue;
        }

        uint64_t originalCount = sem->waitingCount;
        uint64_t kept = 0;

        for (uint64_t j = 0; j < originalCount; j++) {
            uint64_t readIndex =
                (sem->waitingHead + j) % SEMAPHORE_MAX_WAITERS;

            uint64_t waitingPid = sem->waitingPids[readIndex];

            if (waitingPid == pid) {
                removed = 1;
                continue;
            }

            uint64_t writeIndex =
                (sem->waitingHead + kept) % SEMAPHORE_MAX_WAITERS;

            sem->waitingPids[writeIndex] = waitingPid;
            kept++;
        }

        /* Limpiar las posiciones que quedaron libres. */
        for (uint64_t j = kept; j < originalCount; j++) {
            uint64_t index =
                (sem->waitingHead + j) % SEMAPHORE_MAX_WAITERS;

            sem->waitingPids[index] = 0;
        }

        sem->waitingCount = kept;
        sem->waitingTail =
            (sem->waitingHead + kept) % SEMAPHORE_MAX_WAITERS;
    }

    leaveCriticalSection(flags);
    return removed;
}
