#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include <semaphore.h>

/*
 * Verifica la inicializacion de la tabla.
 */
static void testInitialization(void) {
    initializeSemaphoreTable();

    for (uint64_t i = 0; i < MAX_SEMAPHORES; i++) {
        Semaphore *sem = getSemaphoreSlot(i);

        assert(sem != NULL);
        assert(sem->inUse == 0);
        assert(sem->id == 0);
        assert(sem->value == 0);
        assert(sem->openCount == 0);
        assert(sem->waitingHead == 0);
        assert(sem->waitingTail == 0);
        assert(sem->waitingCount == 0);

        for (uint64_t j = 0;
             j < SEMAPHORE_NAME_MAX_LENGTH; j++) {
            assert(sem->name[j] == '\0');
        }

        for (uint64_t j = 0;
             j < SEMAPHORE_MAX_WAITERS; j++) {
            assert(sem->waitingPids[j] == 0);
        }
    }

    assert(getSemaphoreSlot(MAX_SEMAPHORES) == NULL);
    assert(getSemaphoreSlot(UINT64_MAX) == NULL);

    printf("OK: inicializacion de tabla superada\n");
}

/*
 * Verifica entradas invalidas y FIFO.
 */
static void testQueue(void) {
    initializeSemaphoreTable();

    Semaphore *sem = getSemaphoreSlot(0);
    assert(sem != NULL);

    /* No se permite usar un slot inactivo. */
    assert(enqueueSemaphoreWaiter(sem, 10) == 0);
    assert(dequeueSemaphoreWaiter(sem) == 0);

    /* Simular un semaforo abierto. */
    sem->inUse = 1;
    sem->id = 1;

    assert(enqueueSemaphoreWaiter(NULL, 10) == 0);
    assert(dequeueSemaphoreWaiter(NULL) == 0);
    assert(enqueueSemaphoreWaiter(sem, 0) == 0);

    assert(dequeueSemaphoreWaiter(sem) == 0);

    assert(enqueueSemaphoreWaiter(sem, 10) == 1);
    assert(enqueueSemaphoreWaiter(sem, 20) == 1);
    assert(enqueueSemaphoreWaiter(sem, 30) == 1);

    /* No debe insertar dos veces el mismo PID. */
    assert(enqueueSemaphoreWaiter(sem, 20) == 0);
    assert(sem->waitingCount == 3);

    assert(dequeueSemaphoreWaiter(sem) == 10);
    assert(dequeueSemaphoreWaiter(sem) == 20);
    assert(dequeueSemaphoreWaiter(sem) == 30);

    assert(sem->waitingCount == 0);
    assert(dequeueSemaphoreWaiter(sem) == 0);

    printf("OK: cola FIFO y validaciones superadas\n");
}

/*
 * Verifica capacidad maxima y reutilizacion circular.
 */
static void testCapacityAndWrapAround(void) {
    initializeSemaphoreTable();

    Semaphore *sem = getSemaphoreSlot(1);
    assert(sem != NULL);

    sem->inUse = 1;
    sem->id = 2;

    /* Llenar completamente la cola. */
    for (uint64_t pid = 1;
         pid <= SEMAPHORE_MAX_WAITERS; pid++) {
        assert(enqueueSemaphoreWaiter(sem, pid) == 1);
    }

    assert(sem->waitingCount == SEMAPHORE_MAX_WAITERS);

    /* La cola llena debe rechazar nuevas entradas. */
    assert(enqueueSemaphoreWaiter(
        sem, SEMAPHORE_MAX_WAITERS + 1
    ) == 0);

    /* Retirar la primera mitad. */
    uint64_t half = SEMAPHORE_MAX_WAITERS / 2;

    for (uint64_t pid = 1; pid <= half; pid++) {
        assert(dequeueSemaphoreWaiter(sem) == pid);
    }

    /* Volver a llenar la cola: el tail debe circular. */
    for (uint64_t pid = SEMAPHORE_MAX_WAITERS + 1;
         pid <= SEMAPHORE_MAX_WAITERS + half;
         pid++) {
        assert(enqueueSemaphoreWaiter(sem, pid) == 1);
    }

    assert(sem->waitingCount == SEMAPHORE_MAX_WAITERS);

    /* Comprobar el orden luego de reutilizar posiciones. */
    for (uint64_t pid = half + 1;
         pid <= SEMAPHORE_MAX_WAITERS + half;
         pid++) {
        assert(dequeueSemaphoreWaiter(sem) == pid);
    }

    assert(sem->waitingCount == 0);
    assert(dequeueSemaphoreWaiter(sem) == 0);

    printf("OK: capacidad y cola circular superadas\n");
}

/*
 * Verifica aislamiento entre semaforos y reinicializacion.
 */
static void testIsolationAndReset(void) {
    initializeSemaphoreTable();

    Semaphore *first = getSemaphoreSlot(0);
    Semaphore *second = getSemaphoreSlot(1);

    assert(first != NULL);
    assert(second != NULL);
    assert(first != second);

    first->inUse = 1;
    second->inUse = 1;

    first->value = 3;
    first->openCount = 2;
    first->name[0] = 'A';

    assert(enqueueSemaphoreWaiter(first, 50) == 1);
    assert(enqueueSemaphoreWaiter(second, 60) == 1);

    assert(dequeueSemaphoreWaiter(second) == 60);
    assert(dequeueSemaphoreWaiter(first) == 50);

    assert(enqueueSemaphoreWaiter(first, 70) == 1);

    /* Reinicializar debe borrar el estado anterior. */
    initializeSemaphoreTable();

    assert(first->inUse == 0);
    assert(first->value == 0);
    assert(first->openCount == 0);
    assert(first->name[0] == '\0');
    assert(first->waitingCount == 0);
    assert(first->waitingPids[0] == 0);

    assert(second->inUse == 0);
    assert(second->waitingCount == 0);

    printf("OK: aislamiento y reinicializacion superados\n");
}

int main(void) {
    testInitialization();
    testQueue();
    testCapacityAndWrapAround();
    testIsolationAndReset();

    printf("OK: todas las pruebas de semaforos superadas\n");
    return 0;
}
