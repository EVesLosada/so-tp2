#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <process.h>
#include <processTable.h>
#include <processManager.h>
#include <memoryManager.h>

#define POOL_SIZE (4UL * 1024 * 1024)

static uint64_t memoryPool[POOL_SIZE / sizeof(uint64_t)];

static void dummyEntry(void) {
}

int main(void) {
    initializeMemoryManager(memoryPool, sizeof(memoryPool));
    initializeProcessManager();

    MemoryInfo before, during, after;
    getMemoryInfo(&before);

    uint64_t pid1 = createProcess("proceso1", dummyEntry, 1, 0, 1);
    uint64_t pid2 = createProcess("proceso2", dummyEntry, 2, 0, 0);

    assert(pid1 == 1);
    assert(pid2 == 2);
    assert(getProcessCount() == 2);

    Process *p1 = findProcessByPid(pid1);
    Process *p2 = findProcessByPid(pid2);

    assert(p1 != NULL && p2 != NULL);
    assert(p1->state == PROCESS_READY);
    assert(p2->state == PROCESS_READY);
    assert(p1->stackBase != p2->stackBase);
    assert(p1->savedStackPointer != p2->savedStackPointer);

    getMemoryInfo(&during);
    assert(during.freeMemory < before.freeMemory);

    assert(unregisterProcess(pid1) == 1);
    assert(unregisterProcess(pid2) == 1);

    freeMemory(p1->stackBase);
    freeMemory(p2->stackBase);
    freeMemory(p1);
    freeMemory(p2);

    getMemoryInfo(&after);

    assert(getProcessCount() == 0);
    assert(after.freeMemory == before.freeMemory);
    assert(after.usedMemory == before.usedMemory);

    puts("OK: creacion de procesos con Memory Manager real");
    return 0;
}
