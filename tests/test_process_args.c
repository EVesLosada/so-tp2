#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <process.h>
#include <processManager.h>
#include <processTable.h>
#include <memoryManager.h>
#include <registers.h>

#define POOL_SIZE (4UL * 1024 * 1024)

static uint64_t memoryPool[POOL_SIZE / sizeof(uint64_t)];

static void dummyEntry(uint64_t argc, char **argv) {
    (void)argc;
    (void)argv;
}

/* Verifica que un puntero pertenezca al stack del proceso. */
static void checkStackPointer(Process *p, const void *ptr) {
    uintptr_t start = (uintptr_t)p->stackBase;
    uintptr_t end = start + p->stackSize;
    uintptr_t address = (uintptr_t)ptr;

    assert(address >= start);
    assert(address < end);
}

static void releaseProcess(uint64_t pid) {
    Process *p = findProcessByPid(pid);

    assert(p != NULL);
    assert(unregisterProcess(pid) == 1);

    freeMemory(p->stackBase);
    freeMemory(p);
}

int main(void) {
    initializeMemoryManager(memoryPool, sizeof(memoryPool));
    initializeProcessManager();

    MemoryInfo initial, beforeFailures, afterFailures, final;
    getMemoryInfo(&initial);

    /* Crear un proceso con dos argumentos. */
    char first[] = "hola";
    char second[] = "mundo";
    const char *args[] = {first, second};

    uint64_t pid = createProcessWithArgs(
        "con-args", dummyEntry, 2, 0, 1, 2, args
    );

    assert(pid == 1);

    Process *p = findProcessByPid(pid);

    assert(p != NULL);
    assert(p->state == PROCESS_READY);
    assert(p->entryPointWithArgs == dummyEntry);
    assert(p->entryPoint == NULL);
    assert(p->argc == 2);
    assert(p->argv != NULL);
    assert(strcmp(p->argv[0], "hola") == 0);
    assert(strcmp(p->argv[1], "mundo") == 0);
    assert(p->argv[2] == NULL);

    /* Los argumentos deben estar dentro del stack propio. */
    checkStackPointer(p, p->argv);
    checkStackPointer(p, p->argv[0]);
    checkStackPointer(p, p->argv[1]);

    /* Verificar que se hayan copiado independientemente. */
    first[0] = 'X';
    second[0] = 'Y';

    assert(strcmp(p->argv[0], "hola") == 0);
    assert(strcmp(p->argv[1], "mundo") == 0);
    assert(p->argv[0] != first);
    assert(p->argv[1] != second);

    /* Contexto inicial conservado correctamente. */
    RegisterFrame *frame =
        (RegisterFrame *)(uintptr_t)p->savedStackPointer;

    assert(frame->rip != 0);
    assert(frame->rdi == (uint64_t)(uintptr_t)p);
    assert((frame->rsp & 15) == 8);
    assert((uintptr_t)p->argv > frame->rsp);

    /* argc = 0 también debe ser válido. */
    uint64_t emptyPid = createProcessWithArgs(
        "sin-args", dummyEntry, 1, 0, 0, 0, NULL
    );

    assert(emptyPid == 2);

    Process *empty = findProcessByPid(emptyPid);

    assert(empty != NULL);
    assert(empty->argc == 0);
    assert(empty->argv != NULL);
    assert(empty->argv[0] == NULL);

    /* Entradas inválidas no deben crear procesos. */
    getMemoryInfo(&beforeFailures);

    assert(createProcessWithArgs(
        "invalido", NULL, 1, 0, 0, 0, NULL
    ) == 0);

    assert(createProcessWithArgs(
        "invalido", dummyEntry, 1, 0, 0, 1, NULL
    ) == 0);

    assert(createProcessWithArgs(
        "invalido", dummyEntry, 1, 0, 0, 17, args
    ) == 0);

    const char *missing[] = {"valido", NULL};

    assert(createProcessWithArgs(
        "invalido", dummyEntry, 1, 0, 0, 2, missing
    ) == 0);

    char oversized[257];

    memset(oversized, 'A', 256);
    oversized[256] = '\0';

    const char *longArgs[] = {oversized};

    assert(createProcessWithArgs(
        "invalido", dummyEntry, 1, 0, 0, 1, longArgs
    ) == 0);

    getMemoryInfo(&afterFailures);

    assert(getProcessCount() == 2);
    assert(afterFailures.freeMemory == beforeFailures.freeMemory);
    assert(afterFailures.usedMemory == beforeFailures.usedMemory);

    /* Los errores no deben consumir identificadores PID. */
    uint64_t thirdPid = createProcessWithArgs(
        "tercero", dummyEntry, 1, 0, 0, 0, NULL
    );

    assert(thirdPid == 3);

    /* Liberar todo y comprobar que se recupera la memoria. */
    releaseProcess(pid);
    releaseProcess(emptyPid);
    releaseProcess(thirdPid);

    getMemoryInfo(&final);

    assert(getProcessCount() == 0);
    assert(final.freeMemory == initial.freeMemory);
    assert(final.usedMemory == initial.usedMemory);

    puts("OK: pruebas de argc/argv superadas");
    return 0;
}
