#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <shellPrograms.h>
#include <syscalls.h>

#define OUTPUT_CAPACITY 4096

static char output[OUTPUT_CAPACITY];
static uint64_t outputLength;

static uint8_t memorySuccess = 1;
static uint8_t psHasProcesses = 1;

static uint64_t fakeTicks;
static uint64_t yieldCount;
static jmp_buf loopJump;

static void resetOutput(void) {
    outputLength = 0;
    output[0] = '\0';
}

/* Simulacion de escritura en la terminal. */
uint64_t sysWrite(
    uint64_t fd,
    const char *buffer,
    uint64_t count
) {
    assert(fd == STDOUT);
    assert(buffer != NULL);
    assert(outputLength + count < OUTPUT_CAPACITY);

    memcpy(output + outputLength, buffer, (size_t)count);

    outputLength += count;
    output[outputLength] = '\0';

    return count;
}

/* Necesario para enlazar el programa cat. */
uint64_t sysRead(
    uint64_t fd,
    char *buffer,
    uint64_t count
) {
    (void)fd;
    (void)buffer;
    (void)count;
    return 0;
}

/* Simulacion de informacion de memoria. */
uint64_t sysMemInfo(MemoryInfo *info) {
    if (!memorySuccess) {
        return 0;
    }

    info->totalMemory = 1000;
    info->usedMemory = 250;
    info->freeMemory = 750;

    return 1;
}

/* Simulacion del listado de procesos. */
uint64_t sysPs(ProcessInfo *buffer, uint64_t capacity) {
    if (!psHasProcesses) {
        return 0;
    }

    assert(buffer != NULL);
    assert(capacity >= 2);

    memset(buffer, 0, 2 * sizeof(ProcessInfo));

    buffer[0].pid = 2;
    strcpy(buffer[0].name, "loop");
    buffer[0].state = PROCESS_READY;
    buffer[0].priority = 5;
    buffer[0].foreground = 0;
    buffer[0].stackPointer = 0x1234;
    buffer[0].basePointer = 0x5678;

    buffer[1].pid = 3;
    strcpy(buffer[1].name, "ps");
    buffer[1].state = PROCESS_RUNNING;
    buffer[1].priority = 1;
    buffer[1].foreground = 1;

    return 2;
}

/* Simulacion de PID, ticks y yield para loop. */
uint64_t sysGetPid(void) {
    return 42;
}

uint64_t sysGetTicks(void) {
    return fakeTicks++;
}

uint64_t sysYield(void) {
    yieldCount++;

    if (yieldCount >= 105) {
        longjmp(loopJump, 1);
    }

    return 1;
}

static void testMem(void) {
    resetOutput();
    memorySuccess = 1;

    shellMemProcess(0, NULL);

    assert(strcmp(
        output,
        "Memoria total: 1000 bytes\n"
        "Memoria usada: 250 bytes\n"
        "Memoria libre: 750 bytes\n"
    ) == 0);

    resetOutput();
    memorySuccess = 0;

    shellMemProcess(0, NULL);

    assert(strstr(
        output,
        "ERROR: no se pudo consultar la memoria"
    ) != NULL);

    puts("OK: mem muestra valores y gestiona errores");
}

static void testPs(void) {
    resetOutput();
    psHasProcesses = 1;

    shellPsProcess(0, NULL);

    assert(strstr(output, "Procesos: 2\n") != NULL);

    assert(strstr(
        output,
        "PID: 2 | Nombre: loop | Estado: READY"
        " | Prioridad: 5 | BG"
    ) != NULL);

    assert(strstr(
        output,
        "RSP: 0x0000000000001234"
        " | RBP: 0x0000000000005678"
    ) != NULL);

    assert(strstr(
        output,
        "PID: 3 | Nombre: ps | Estado: RUNNING"
        " | Prioridad: 1 | FG"
    ) != NULL);

    resetOutput();
    psHasProcesses = 0;

    shellPsProcess(0, NULL);

    assert(strcmp(output, "Procesos: 0\n") == 0);

    puts("OK: ps muestra procesos y lista vacia");
}

static void testLoop(void) {
    char *argv[] = {"loop", NULL};

    resetOutput();

    /* Rechazar argumentos adicionales. */
    shellLoopProcess(2, argv);

    assert(strcmp(output, "Uso: loop\n") == 0);

    resetOutput();
    fakeTicks = 0;
    yieldCount = 0;

    /*
     * Simulamos 105 cesiones de CPU.
     * longjmp permite finalizar el test de un loop infinito.
     */
    if (setjmp(loopJump) == 0) {
        shellLoopProcess(1, argv);
        assert(0 && "loop no deberia finalizar solo");
    }

    assert(yieldCount >= 100);

    assert(strstr(
        output,
        "loop iniciado - PID 42\n"
    ) != NULL);

    assert(strstr(
        output,
        "loop ejecutando - PID 42\n"
    ) != NULL);

    puts("OK: loop usa ticks, PID y yield");
}

int main(void) {
    testMem();
    testPs();
    testLoop();

    puts("OK: test_shell_programs");
    return 0;
}
