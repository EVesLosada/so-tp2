#include <stdint.h>
#include <stddef.h>

#include <processManager.h>
#include <processTable.h>
#include <memoryManager.h>
#include <registers.h>

#define KERNEL_CODE_SELECTOR 0x08ULL
#define INITIAL_RFLAGS 0x202ULL
#define STACK_ALIGNMENT 16ULL

#define MAX_PROCESS_ARGUMENTS 16ULL
#define MAX_PROCESS_ARGUMENT_LENGTH 256ULL
#define MIN_EXECUTION_STACK 4096ULL

static uint64_t nextPid = 1;

/*
 * Punto de entrada inicial de todos los procesos.
 */
static void processBootstrap(Process *process) {
    if (process->entryPointWithArgs != NULL) {
        process->entryPointWithArgs(process->argc, process->argv);
    } else {
        process->entryPoint();
    }

    /*
     * La finalizacion completa corresponde al Bloque 10.
     */
    process->state = PROCESS_TERMINATED;

    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void initializeProcessManager(void) {
    initializeProcessTable();
    nextPid = 1;
}

/*
 * Copia argc y argv a la parte alta del stack nuevo.
 *
 * Las cadenas quedan por encima del RSP inicial.
 * Como el stack crece hacia abajo, no son sobrescritas
 * por las llamadas normales a funciones.
 */
static uint8_t copyProcessArguments(
    Process *process,
    uint64_t argc,
    const char *const argv[],
    uintptr_t *top
) {
    if (argc > MAX_PROCESS_ARGUMENTS ||
        (argc != 0 && argv == NULL)) {
        return 0;
    }

    char *copied[MAX_PROCESS_ARGUMENTS];

    for (uint64_t i = argc; i > 0; i--) {
        const char *source = argv[i - 1];

        if (source == NULL) {
            return 0;
        }

        uint64_t length = 0;

        while (length < MAX_PROCESS_ARGUMENT_LENGTH &&
               source[length] != '\0') {
            length++;
        }

        if (length == MAX_PROCESS_ARGUMENT_LENGTH) {
            return 0;
        }

        *top -= length + 1;

        char *destination = (char *)(uintptr_t)*top;

        for (uint64_t j = 0; j <= length; j++) {
            destination[j] = source[j];
        }

        copied[i - 1] = destination;
    }

    *top &= ~(uintptr_t)(sizeof(char *) - 1);
    *top -= (argc + 1) * sizeof(char *);

    char **newArgv = (char **)(uintptr_t)*top;

    for (uint64_t i = 0; i < argc; i++) {
        newArgv[i] = copied[i];
    }

    newArgv[argc] = NULL;

    process->argc = argc;
    process->argv = newArgv;

    return 1;
}

/*
 * Implementacion comun para crear procesos
 * con y sin argumentos.
 */
static uint64_t createProcessInternal(
    const char *name,
    void (*simpleEntry)(void),
    void (*argsEntry)(uint64_t, char **),
    uint64_t priority,
    uint64_t parentPid,
    uint8_t foreground,
    uint64_t argc,
    const char *const argv[]
) {
    if (name == NULL ||
        name[0] == '\0' ||
        (simpleEntry == NULL && argsEntry == NULL) ||
        nextPid == UINT64_MAX ||
        getProcessCount() >= MAX_PROCESSES) {
        return 0;
    }

    if (argsEntry != NULL &&
        (argc > MAX_PROCESS_ARGUMENTS ||
         (argc != 0 && argv == NULL))) {
        return 0;
    }

    Process *process = allocateMemory(sizeof(Process));

    if (process == NULL) {
        return 0;
    }

    void *stack = allocateMemory(PROCESS_STACK_SIZE);

    if (stack == NULL) {
        freeMemory(process);
        return 0;
    }

    *process = (Process){0};

    process->pid = nextPid;
    process->priority = priority;
    process->parentPid = parentPid;
    process->foreground = foreground != 0;
    process->state = PROCESS_READY;

    process->stackBase = stack;
    process->stackSize = PROCESS_STACK_SIZE;

    process->entryPoint = simpleEntry;
    process->entryPointWithArgs = argsEntry;

    /*
     * Copiar el nombre al PCB.
     */
    uint64_t i = 0;

    while (i < PROCESS_NAME_MAX_LENGTH - 1 &&
           name[i] != '\0') {
        process->name[i] = name[i];
        i++;
    }

    process->name[i] = '\0';

    uintptr_t stackStart = (uintptr_t)stack;
    uintptr_t top = stackStart + PROCESS_STACK_SIZE;

    /*
     * Si recibe argumentos, copiarlos al nuevo stack.
     */
    if (argsEntry != NULL &&
        !copyProcessArguments(process, argc, argv, &top)) {
        freeMemory(stack);
        freeMemory(process);
        return 0;
    }

    /*
     * Preparar el contexto que restaurara el futuro
     * mecanismo de context switching.
     */
    uintptr_t entryRsp =
        (top & ~(uintptr_t)(STACK_ALIGNMENT - 1)) -
        sizeof(uint64_t);

    uintptr_t savedFrame =
        entryRsp - sizeof(RegisterFrame);

    /*
     * Conservar espacio suficiente para la ejecucion.
     */
    if (savedFrame < stackStart + MIN_EXECUTION_STACK) {
        freeMemory(stack);
        freeMemory(process);
        return 0;
    }

    *(uint64_t *)entryRsp = 0;

    RegisterFrame *frame = (RegisterFrame *)savedFrame;

    *frame = (RegisterFrame){0};

    frame->rsp = entryRsp;
    frame->rip = (uint64_t)(uintptr_t)processBootstrap;
    frame->cs = KERNEL_CODE_SELECTOR;
    frame->rflags = INITIAL_RFLAGS;
    frame->rdi = (uint64_t)(uintptr_t)process;

    process->savedStackPointer = (uint64_t)savedFrame;

    if (!registerProcess(process)) {
        freeMemory(stack);
        freeMemory(process);
        return 0;
    }

    return nextPid++;
}

/*
 * Mantener la interfaz original sin argumentos.
 */
uint64_t createProcess(
    const char *name,
    void (*entryPoint)(void),
    uint64_t priority,
    uint64_t parentPid,
    uint8_t foreground
) {
    return createProcessInternal(
        name,
        entryPoint,
        NULL,
        priority,
        parentPid,
        foreground,
        0,
        NULL
    );
}

/*
 * Nueva interfaz para procesos con argumentos.
 */
uint64_t createProcessWithArgs(
    const char *name,
    void (*entryPoint)(uint64_t argc, char **argv),
    uint64_t priority,
    uint64_t parentPid,
    uint8_t foreground,
    uint64_t argc,
    const char *const argv[]
) {
    if (entryPoint == NULL) {
        return 0;
    }

    return createProcessInternal(
        name,
        NULL,
        entryPoint,
        priority,
        parentPid,
        foreground,
        argc,
        argv
    );
}
