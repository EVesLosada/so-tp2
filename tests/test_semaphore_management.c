#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <semaphore.h>

static Semaphore *findSemaphore(uint64_t id) {
    for (uint64_t i = 0; i < MAX_SEMAPHORES; i++) {
        Semaphore *sem = getSemaphoreSlot(i);

        if (sem->inUse && sem->id == id) {
            return sem;
        }
    }

    return NULL;
}

static void testCreateAndOpen(void) {
    initializeSemaphoreTable();

    uint64_t id = semCreate("mutex", 1);

    assert(id != 0);

    Semaphore *sem = findSemaphore(id);

    assert(sem != NULL);
    assert(sem->value == 1);
    assert(sem->openCount == 1);

    uint64_t openedId = semOpen("mutex");

    assert(openedId == id);
    assert(sem->openCount == 2);

    printf("OK: creacion y apertura\n");
}

static void testDuplicateNames(void) {
    initializeSemaphoreTable();

    uint64_t id = semCreate("shared", 3);

    assert(id != 0);
    assert(semCreate("shared", 10) == 0);

    Semaphore *sem = findSemaphore(id);

    assert(sem != NULL);
    assert(sem->value == 3);
    assert(sem->openCount == 1);

    printf("OK: nombres duplicados\n");
}

static void testClose(void) {
    initializeSemaphoreTable();

    uint64_t id = semCreate("close-test", 2);

    assert(id != 0);
    assert(semOpen("close-test") == id);

    Semaphore *sem = findSemaphore(id);

    assert(sem != NULL);
    assert(sem->openCount == 2);

    assert(semClose(id) == 1);
    assert(sem->openCount == 1);
    assert(sem->inUse == 1);

    assert(semClose(id) == 1);
    assert(sem->inUse == 0);

    assert(semClose(id) == 0);
    assert(semOpen("close-test") == 0);

    printf("OK: cierre y liberacion\n");
}

static void testInvalidArguments(void) {
    initializeSemaphoreTable();

    assert(semCreate(NULL, 1) == 0);
    assert(semCreate("", 1) == 0);
    assert(semOpen(NULL) == 0);
    assert(semOpen("inexistente") == 0);
    assert(semClose(0) == 0);
    assert(semClose(UINT64_MAX) == 0);

    char longName[SEMAPHORE_NAME_MAX_LENGTH + 1];

    for (uint64_t i = 0; i < SEMAPHORE_NAME_MAX_LENGTH; i++) {
        longName[i] = 'a';
    }

    longName[SEMAPHORE_NAME_MAX_LENGTH] = '\0';

    assert(semCreate(longName, 1) == 0);

    printf("OK: argumentos invalidos\n");
}

static void testCapacity(void) {
    initializeSemaphoreTable();

    char name[] = "sem00";

    for (uint64_t i = 0; i < MAX_SEMAPHORES; i++) {
        name[3] = '0' + (char)(i / 10);
        name[4] = '0' + (char)(i % 10);

        assert(semCreate(name, i) != 0);
    }

    assert(semCreate("extra", 1) == 0);

    printf("OK: capacidad maxima\n");
}

static void testWaitingProcesses(void) {
    initializeSemaphoreTable();

    uint64_t id = semCreate("waiting", 0);

    assert(id != 0);

    Semaphore *sem = findSemaphore(id);

    assert(sem != NULL);
    assert(enqueueSemaphoreWaiter(sem, 42) == 1);

    /* No podemos eliminarlo mientras hay procesos esperando. */
    assert(semClose(id) == 0);
    assert(sem->inUse == 1);
    assert(sem->openCount == 1);

    assert(dequeueSemaphoreWaiter(sem) == 42);
    assert(semClose(id) == 1);

    printf("OK: cierre con procesos esperando\n");
}

int main(void) {
    testCreateAndOpen();
    testDuplicateNames();
    testClose();
    testInvalidArguments();
    testCapacity();
    testWaitingProcesses();

    printf("OK: tests de administracion de semaforos superados\n");
    return 0;
}
