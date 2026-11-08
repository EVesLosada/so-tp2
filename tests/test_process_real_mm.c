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

        /*
     * Verificar reservas de Userland asociadas a un proceso
     * utilizando el administrador de memoria real.
     */
    uint64_t cleanupPid = createProcess(
        "memoria_cleanup", dummyEntry, 1, 0, 0
    );
    assert(cleanupPid != 0);

    uint64_t shellRsp = 0x55550000ULL;
    uint64_t processRsp = schedule(shellRsp);

    assert(getCurrentProcess() != NULL);
    assert(getCurrentProcess()->pid == cleanupPid);

    uint8_t *block1 = (uint8_t *)allocateProcessMemory(128);
    uint8_t *block2 = (uint8_t *)allocateProcessMemory(512);
    uint8_t *block3 = (uint8_t *)allocateProcessMemory(1024);

    assert(block1 != NULL);
    assert(block2 != NULL);
    assert(block3 != NULL);

    for (uint64_t i = 0; i < 128; i++) {
        block1[i] = 0xAA;
    }
    for (uint64_t i = 0; i < 512; i++) {
        block2[i] = 0x55;
    }
    for (uint64_t i = 0; i < 1024; i++) {
        block3[i] = 0xCC;
    }

    for (uint64_t i = 0; i < 128; i++) {
        assert(block1[i] == 0xAA);
    }
    for (uint64_t i = 0; i < 512; i++) {
        assert(block2[i] == 0x55);
    }
    for (uint64_t i = 0; i < 1024; i++) {
        assert(block3[i] == 0xCC);
    }

    /* Liberar una reserva manualmente. */
    freeProcessMemory(block1);

    /* Las otras dos deben liberarse al terminar el proceso. */
    assert(exitCurrentProcess() == 1);
    assert(schedule(processRsp) == shellRsp);
    assert(getCurrentProcess() == NULL);

    assert(reapTerminatedProcesses() == 1);
    assert(findProcessByPid(cleanupPid) == NULL);

    MemoryInfo cleanupAfter;
    getMemoryInfo(&cleanupAfter);

    assert(cleanupAfter.freeMemory == before.freeMemory);
    assert(cleanupAfter.usedMemory == before.usedMemory);

    puts("OK: limpieza automatica de memoria con MM real");

    puts("OK: creacion de procesos con Memory Manager real");
    return 0;
}
