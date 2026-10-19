#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <process.h>
#include <processTable.h>
#include <processManager.h>
#include <memoryManager.h>
#include <registers.h>

/*
 * Simulacion del Memory Manager para las pruebas.
 * processManager.c y processTable.c son los reales.
 */
static uint64_t allocationCalls = 0;
static uint64_t activeAllocations = 0;
static uint64_t failOnCall = 0;

void *allocateMemory(uint64_t size) {
    allocationCalls++;

    if (failOnCall == allocationCalls) {
        return NULL;
    }

    void *ptr = malloc((size_t)size);

    if (ptr != NULL) {
        activeAllocations++;
    }

    return ptr;
}

void freeMemory(void *ptr) {
    if (ptr != NULL) {
        assert(activeAllocations > 0);
        activeAllocations--;
        free(ptr);
    }
}

static void dummyEntry(void) {
}

static void checkProcessContext(Process *process) {
    assert(process != NULL);
    assert(process->state == PROCESS_READY);
    assert(process->stackBase != NULL);
    assert(process->stackSize == PROCESS_STACK_SIZE);

    uintptr_t start = (uintptr_t)process->stackBase;
    uintptr_t end = start + process->stackSize;
    uintptr_t saved = process->savedStackPointer;

    assert(saved >= start);
    assert(saved + sizeof(RegisterFrame) <= end);

    RegisterFrame *frame = (RegisterFrame *)saved;

    assert(frame->rip != 0);
    assert(frame->cs == 0x08);
    assert(frame->rflags == 0x202);
    assert(frame->rdi == (uint64_t)(uintptr_t)process);
    assert(frame->rsp == saved + sizeof(RegisterFrame));
    assert((frame->rsp & 15) == 8);
}

int main(void) {
    initializeProcessManager();

    /* Tabla inicialmente vacia. */
    assert(getProcessCount() == 0);

    /* Rechazar parametros invalidos. */
    assert(createProcess(NULL, dummyEntry, 1, 0, 1) == 0);
    assert(createProcess("", dummyEntry, 1, 0, 1) == 0);
    assert(createProcess("test", NULL, 1, 0, 1) == 0);
    assert(getProcessCount() == 0);

    /* Fallo al reservar PCB. */
    failOnCall = allocationCalls + 1;

    assert(createProcess("test", dummyEntry, 1, 0, 1) == 0);
    assert(getProcessCount() == 0);
    assert(activeAllocations == 0);

    /* Fallo al reservar stack: debe liberar el PCB. */
    failOnCall = allocationCalls + 2;

    assert(createProcess("test", dummyEntry, 1, 0, 1) == 0);
    assert(getProcessCount() == 0);
    assert(activeAllocations == 0);

    failOnCall = 0;

    /* Crear procesos hasta llenar la tabla. */
    uint64_t pids[MAX_PROCESSES];

    for (uint64_t i = 0; i < MAX_PROCESSES; i++) {
        pids[i] = createProcess(
            "proceso",
            dummyEntry,
            2,
            0,
            1
        );

        assert(pids[i] == i + 1);

        Process *process = findProcessByPid(pids[i]);

        assert(process != NULL);
        assert(process->pid == pids[i]);
        assert(process->priority == 2);
        assert(process->parentPid == 0);
        assert(process->foreground == 1);
        assert(strcmp(process->name, "proceso") == 0);
        assert(process->entryPoint == dummyEntry);

        checkProcessContext(process);
    }

    assert(getProcessCount() == MAX_PROCESSES);
    assert(activeAllocations == MAX_PROCESSES * 2);

    /* Comprobar que los primeros stacks son independientes. */
    Process *first = findProcessByPid(pids[0]);
    Process *second = findProcessByPid(pids[1]);

    assert(first->stackBase != second->stackBase);
    assert(first->savedStackPointer != second->savedStackPointer);

    /* No debe aceptar un proceso adicional. */
    assert(createProcess("extra", dummyEntry, 1, 0, 1) == 0);
    assert(getProcessCount() == MAX_PROCESSES);

    /*
     * Limpieza desde el test.
     * La destruccion definitiva corresponde al Bloque 10.
     */
    for (uint64_t i = 0; i < MAX_PROCESSES; i++) {
        Process *process = findProcessByPid(pids[i]);

        assert(process != NULL);
        assert(unregisterProcess(pids[i]) == 1);

        freeMemory(process->stackBase);
        freeMemory(process);
    }

    assert(getProcessCount() == 0);
    assert(activeAllocations == 0);

    puts("OK: pruebas del administrador de procesos superadas");
    return 0;
}
