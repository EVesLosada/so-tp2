#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <semaphore.h>
#include <processManager.h>

/*
 * Simula el desbloqueo de procesos.
 * Los PIDs 10 y 20 representan procesos bloqueados.
 */
static uint64_t lastUnblocked = 0;

uint8_t unblockProcessFromSemaphore(uint64_t pid, uint64_t semaphoreId) {
    lastUnblocked = pid;
    return semaphoreId != 0 && (pid == 10 || pid == 20);
}


/*
 * Simulaciones necesarias para enlazar semaphoreSync.c.
 * Este test solo verifica semPost.
 */
Process *getCurrentProcess(void) {
    return 0;
}

uint8_t blockProcess(uint64_t pid) {
    (void)pid;
    return 0;
}

int main(void) {
    initializeSemaphoreTable();

    uint64_t id = semCreate("test_post", 0);
    assert(id != 0);

    Semaphore *sem = NULL;

    for (uint64_t i = 0; i < MAX_SEMAPHORES; i++) {
        Semaphore *candidate = getSemaphoreSlot(i);

        if (candidate->inUse && candidate->id == id) {
            sem = candidate;
            break;
        }
    }

    assert(sem != NULL);

    /* Sin procesos esperando, incrementa el contador. */
    assert(semPost(id) == 1);
    assert(sem->value == 1);

    /* Preparar dos procesos esperando. */
    sem->value = 0;

    assert(enqueueSemaphoreWaiter(sem, 10));
    assert(enqueueSemaphoreWaiter(sem, 20));

    /* Debe despertar primero al PID 10. */
    assert(semPost(id) == 1);
    assert(lastUnblocked == 10);
    assert(sem->waitingCount == 1);
    assert(sem->value == 0);

    /* Despues al PID 20. */
    assert(semPost(id) == 1);
    assert(lastUnblocked == 20);
    assert(sem->waitingCount == 0);
    assert(sem->value == 0);

    /* Cola vacia: vuelve a incrementar el contador. */
    assert(semPost(id) == 1);
    assert(sem->value == 1);

    /* ID invalido. */
    assert(semPost(0) == 0);

    /* Semaforo cerrado. */
    assert(semClose(id) == 1);
    assert(semPost(id) == 0);

    printf("OK: semPost incrementa y respeta FIFO\n");

    return 0;
}
