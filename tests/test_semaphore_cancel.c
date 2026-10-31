#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <semaphore.h>

int main(void) {
    initializeSemaphoreTable();

    uint64_t id = semCreate("cancel-test", 0);
    assert(id != 0);

    Semaphore *sem = 0;

    for (uint64_t i = 0; i < MAX_SEMAPHORES; i++) {
        Semaphore *candidate = getSemaphoreSlot(i);
        if (candidate->inUse && candidate->id == id) {
            sem = candidate;
            break;
        }
    }

    assert(sem != 0);

    assert(enqueueSemaphoreWaiter(sem, 10));
    assert(enqueueSemaphoreWaiter(sem, 20));
    assert(enqueueSemaphoreWaiter(sem, 30));

    /* Eliminar al proceso del medio. */
    assert(semCancelWaiter(20) == 1);
    assert(sem->waitingCount == 2);

    /* Los demas mantienen su orden. */
    assert(dequeueSemaphoreWaiter(sem) == 10);
    assert(dequeueSemaphoreWaiter(sem) == 30);
    assert(sem->waitingCount == 0);

    /* Eliminar un PID inexistente no cambia la cola. */
    assert(semCancelWaiter(99) == 0);

    /* Eliminar al unico proceso esperando. */
    assert(enqueueSemaphoreWaiter(sem, 40));
    assert(semClose(id) == 0);
    assert(semCancelWaiter(40) == 1);
    assert(sem->waitingCount == 0);

    /* Ahora el cierre puede completarse. */
    assert(semClose(id) == 1);

    printf("OK: limpieza de waiters conserva FIFO\n");
    return 0;
}
