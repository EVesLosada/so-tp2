#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <semaphore.h>
#include <processManager.h>

static Process fakeProcess;
static uint8_t blockingAllowed = 1;
static uint8_t processVisible = 1;

Process *getCurrentProcess(void) {
    return processVisible ? &fakeProcess : NULL;
}

uint8_t blockProcess(uint64_t pid) {
    if (!blockingAllowed ||
        pid != fakeProcess.pid ||
        fakeProcess.state != PROCESS_RUNNING) {
        return 0;
    }

    fakeProcess.state = PROCESS_BLOCKED;
    return 1;
}

uint8_t unblockProcessFromSemaphore(uint64_t pid, uint64_t semaphoreId) {
    if (pid != fakeProcess.pid ||
        fakeProcess.state != PROCESS_BLOCKED ||
        fakeProcess.waitingSemaphoreId != semaphoreId) {
        return 0;
    }

    fakeProcess.waitingSemaphoreId = 0;
    fakeProcess.state = PROCESS_READY;
    return 1;
}

int main(void) {
    initializeSemaphoreTable();

    fakeProcess = (Process){0};
    fakeProcess.pid = 42;
    fakeProcess.state = PROCESS_RUNNING;

    uint64_t id = semCreate("test_wait", 1);
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

    /* Un recurso disponible se consume. */
    assert(semWait(id) == 1);
    assert(sem->value == 0);
    assert(sem->waitingCount == 0);

    /* Sin recursos, el proceso queda encolado y bloqueado. */
    assert(semWait(id) == 1);
    assert(fakeProcess.state == PROCESS_BLOCKED);
    assert(sem->waitingCount == 1);
    assert(sem->waitingPids[sem->waitingHead] == 42);

    /* Post despierta al proceso sin aumentar el contador. */
    assert(semPost(id) == 1);
    assert(fakeProcess.state == PROCESS_READY);
    assert(sem->waitingCount == 0);
    assert(sem->value == 0);

    /* No se permite esperar sin un proceso ejecutandose. */
    processVisible = 0;
    assert(semWait(id) == 0);
    assert(sem->waitingCount == 0);
    processVisible = 1;

    /* Si el bloqueo falla, se revierte el encolado. */
    fakeProcess.state = PROCESS_RUNNING;
    blockingAllowed = 0;

    assert(semWait(id) == 0);
    assert(sem->waitingCount == 0);

    blockingAllowed = 1;

    /* IDs invalidos. */
    assert(semWait(0) == 0);
    assert(semWait(UINT64_MAX) == 0);

    assert(semClose(id) == 1);
    assert(semWait(id) == 0);

    printf("OK: semWait consume, bloquea y respeta FIFO\n");
    return 0;
}
