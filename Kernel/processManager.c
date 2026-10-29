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
static uint64_t nextProcessIndex = 0;
static Process *currentProcess = NULL;
// Contexto de la shell, que todavía se ejecuta sin PCB.
static uint64_t shellSavedStackPointer = 0;

/*
 * Guarda el estado de las interrupciones y las desactiva.
 *
 * En los tests de Linux no ejecutamos CLI porque es
 * una instruccion privilegiada.
 */
static uint64_t saveAndDisableInterrupts(void) {
#if __STDC_HOSTED__
    return 0;
#else
    uint64_t flags;

    __asm__ volatile (
        "pushfq\n\t"
        "popq %0\n\t"
        "cli"
        : "=r"(flags)
        :
        : "memory", "cc"
    );

    return flags;
#endif
}

/*
 * Restaura el estado previo de las interrupciones.
 */
static void restoreInterrupts(uint64_t flags) {
#if __STDC_HOSTED__
    (void)flags;
#else
    __asm__ volatile (
        "pushq %0\n\t"
        "popfq"
        :
        : "r"(flags)
        : "memory", "cc"
    );
#endif
}

/*
 * Punto de entrada inicial de todos los procesos.
 */
/*
 * Despierta al padre si esta esperando a este hijo.
 * Se llama con las interrupciones deshabilitadas.
 */
static void wakeParentWaitingForChild(Process *child) {
    if (child->parentPid == 0) {
        return;
    }

    Process *parent = findProcessByPid(child->parentPid);

    if (parent != NULL &&
        parent->state == PROCESS_BLOCKED &&
        parent->waitingForChildPid == child->pid) {
        parent->state = PROCESS_READY;
    }
}

static void processBootstrap(Process *process) {
    if (process->entryPointWithArgs != NULL) {
        process->entryPointWithArgs(process->argc, process->argv);
    } else {
        process->entryPoint();
    }

    /*
     * Si la funcion del proceso retorna,
     * tambien debe finalizar correctamente.
     */
    exitCurrentProcess();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void initializeProcessManager(void) {
    initializeProcessTable();
    nextPid = 1;
    nextProcessIndex = 0;
    currentProcess = NULL;
    shellSavedStackPointer = 0;
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

    uintptr_t savedFrame = entryRsp - sizeof(RegisterFrame);

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
    frame->iretRsp = entryRsp;
    frame->iretSs = 0x00;
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

Process *selectNextReadyProcess(void) {
    for (uint64_t checked = 0; checked < MAX_PROCESSES; checked++) {
        uint64_t index =
            (nextProcessIndex + checked) % MAX_PROCESSES;

        Process *process = getProcessAt(index);

        if (process != NULL && process->state == PROCESS_READY) {
            nextProcessIndex = (index + 1) % MAX_PROCESSES;
            return process;
        }
    }

    return NULL;
}

uint64_t getProcessSnapshot(ProcessInfo *destination, uint64_t capacity) {
    if (destination == NULL || capacity == 0) {
        return 0;
    }

    uint64_t flags = saveAndDisableInterrupts();
    uint64_t count = 0;

    for (uint64_t i = 0; i < MAX_PROCESSES && count < capacity; i++) {
        Process *process = getProcessAt(i);

        if (process == NULL) {
            continue;
        }

        ProcessInfo *info = &destination[count];
        *info = (ProcessInfo){0};

        info->pid = process->pid;
        info->priority = process->priority;
        info->state = process->state;
        info->foreground = process->foreground;

        /*
         * Contexto guardado por el scheduler.
         * El RBP exacto aun no se registra en el PCB.
         */
        info->stackPointer = process->savedStackPointer;
        info->basePointer = 0;

        for (uint64_t j = 0; j < PROCESS_NAME_MAX_LENGTH; j++) {
            info->name[j] = process->name[j];
        }

        count++;
    }

    restoreInterrupts(flags);
    return count;
}

Process *getCurrentProcess(void) {
    return currentProcess;
}

/*
 * Finaliza el proceso actual sin liberar su stack
 * mientras todavia esta ejecutandose sobre el.
 */
uint8_t exitCurrentProcess(void) {
    uint64_t flags = saveAndDisableInterrupts();

    Process *process = currentProcess;

    if (process == NULL || process->state != PROCESS_RUNNING) {
        restoreInterrupts(flags);
        return 0;
    }

    process->state = PROCESS_TERMINATED;
    wakeParentWaitingForChild(process);

#if __STDC_HOSTED__
    /*
     * En los tests de Linux no se puede ejecutar INT 0x81.
     * Se permite comprobar el cambio de estado.
     */
    restoreInterrupts(flags);
    return 1;
#else
    /*
     * El scheduler guardara el contexto, pero nunca
     * volvera a seleccionar este proceso terminado.
     *
     * Mantenemos las interrupciones deshabilitadas
     * hasta realizar el cambio de contexto.
     */
    __asm__ volatile ("int $0x81" ::: "memory");

    /*
     * No deberia regresar a este contexto.
     */
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
#endif
}

uint8_t setProcessPriority(uint64_t pid, uint64_t priority) {
    uint64_t flags = saveAndDisableInterrupts();

    Process *process = findProcessByPid(pid);

    if (process == NULL ||
        process->state == PROCESS_TERMINATED) {
        restoreInterrupts(flags);
        return 0;
    }

    process->priority = priority;

    restoreInterrupts(flags);
    return 1;
}

uint8_t blockProcess(uint64_t pid) {
    uint64_t flags = saveAndDisableInterrupts();

    Process *process = findProcessByPid(pid);

    if (process == NULL) {
        restoreInterrupts(flags);
        return 0;
    }

    /*
     * Caso 1: bloquear el proceso actualmente
     * en ejecucion.
     */
    if (process == currentProcess &&
        process->state == PROCESS_RUNNING) {

        process->state = PROCESS_BLOCKED;

        /*
         * En el kernel, INT 0x81 guarda el contexto
         * y ejecuta el scheduler inmediatamente.
         *
         * En los tests de Linux no ejecutamos
         * interrupciones del kernel: el cambio de
         * contexto se simula por separado.
         */
#if !__STDC_HOSTED__
        __asm__ volatile ("int $0x81" ::: "memory");
#endif

        /*
         * Si otro proceso nos desbloquea y el
         * scheduler nos reanuda, continuamos aca.
         */
        restoreInterrupts(flags);
        return 1;
    }

    /*
     * Caso 2: bloquear otro proceso que esta READY.
     */
    if (process->state == PROCESS_READY) {
        process->state = PROCESS_BLOCKED;

        restoreInterrupts(flags);
        return 1;
    }

    /*
     * No permitimos bloquear procesos que ya estan
     * bloqueados, terminados o ejecutandose en otro
     * contexto.
     */
    restoreInterrupts(flags);
    return 0;
}

uint8_t unblockProcess(uint64_t pid) {
    uint64_t flags = saveAndDisableInterrupts();

    Process *process = findProcessByPid(pid);
    uint8_t result = 0;

    if (process != NULL &&
        process->state == PROCESS_BLOCKED) {

        process->state = PROCESS_READY;
        result = 1;
    }

    restoreInterrupts(flags);
    return result;
}

uint8_t terminateProcess(uint64_t pid) {
    uint64_t flags = saveAndDisableInterrupts();

    Process *process = findProcessByPid(pid);
    uint8_t result = 0;

    if (process != NULL &&
        process != currentProcess &&
        process->state != PROCESS_RUNNING &&
        process->state != PROCESS_TERMINATED) {

        process->state = PROCESS_TERMINATED;
        wakeParentWaitingForChild(process);
        result = 1;
    }

    restoreInterrupts(flags);
    return result;
}

uint64_t reapTerminatedProcesses(void) {
    uint64_t flags = saveAndDisableInterrupts();
    uint64_t reaped = 0;

    for (uint64_t i = 0; i < MAX_PROCESSES; i++) {
        Process *process = getProcessAt(i);

        if (process == NULL) {
            continue;
        }

        /*
         * Nunca liberar el proceso que todavia
         * esta usando la CPU.
         */
        if (process == currentProcess) {
            continue;
        }

        if (process->state != PROCESS_TERMINATED) {
            continue;
        }

        /*
         * Conservar los hijos terminados de padres vivos
         * hasta que el padre recoja su finalizacion.
         *
         * Los procesos sin padre, huerfanos o cuyos
         * padres terminaron pueden liberarse.
         */
        if (process->parentPid != 0) {
            Process *parent = findProcessByPid(process->parentPid);

            if (parent != NULL &&
                parent->state != PROCESS_TERMINATED) {
                continue;
            }
        }

        uint64_t pid = process->pid;
        void *stack = process->stackBase;

        /*
         * Primero quitar el proceso de la tabla,
         * para que nadie pueda volver a encontrarlo.
         */
        if (!unregisterProcess(pid)) {
            continue;
        }

        /*
         * Ya no esta registrado ni ejecutandose:
         * podemos liberar su stack y su PCB.
         */
        freeMemory(stack);
        freeMemory(process);

        reaped++;
    }

    restoreInterrupts(flags);

    return reaped;
}

/*
 * Espera la finalizacion de un hijo directo.
 */
uint64_t waitForChild(uint64_t childPid) {
    uint64_t flags = saveAndDisableInterrupts();

    Process *parent = currentProcess;

    if (parent == NULL || childPid == 0) {
        restoreInterrupts(flags);
        return 0;
    }

    Process *child = findProcessByPid(childPid);

    if (child == NULL || child->parentPid != parent->pid) {
        restoreInterrupts(flags);
        return 0;
    }

    /*
     * Mientras el hijo no termine, bloquear al padre
     * y ceder la CPU.
     */
    while (child->state != PROCESS_TERMINATED) {
        parent->waitingForChildPid = childPid;
        parent->state = PROCESS_BLOCKED;

#if __STDC_HOSTED__
        /*
         * Los tests de Linux no pueden ejecutar INT 0x81.
         * Devuelve 0 para permitir simular la espera.
         */
        restoreInterrupts(flags);
        return 0;
#else
        __asm__ volatile ("int $0x81" ::: "memory");
#endif
    }

    /*
     * El hijo termino y el padre ya puede recogerlo.
     */
    parent->waitingForChildPid = 0;

    if (!unregisterProcess(childPid)) {
        restoreInterrupts(flags);
        return 0;
    }

    void *stack = child->stackBase;

    freeMemory(stack);
    freeMemory(child);

    restoreInterrupts(flags);
    return childPid;
}

uint64_t schedule(uint64_t savedRsp) {
    Process *previousProcess = currentProcess;

    // Estamos ejecutando la shell, que no tiene PCB.
    if (previousProcess == NULL) {
        shellSavedStackPointer = savedRsp;

        Process *nextProcess = selectNextReadyProcess();

        if (nextProcess == NULL) {
            return savedRsp;
        }

        nextProcess->state = PROCESS_RUNNING;
        currentProcess = nextProcess;

        return nextProcess->savedStackPointer;
    }

    // Guardamos el contexto del proceso interrumpido.
    previousProcess->savedStackPointer = savedRsp;

    // Solo un proceso RUNNING puede volver a READY.
    if (previousProcess->state == PROCESS_RUNNING) {
        previousProcess->state = PROCESS_READY;
    }

    // Alternamos con la shell para que no quede sin CPU.
    if (shellSavedStackPointer != 0 &&
        previousProcess->state == PROCESS_READY) {
        currentProcess = NULL;
        return shellSavedStackPointer;
    }

    Process *nextProcess = selectNextReadyProcess();

    // Si hay otro proceso listo, lo ejecutamos.
    if (nextProcess != NULL) {
        nextProcess->state = PROCESS_RUNNING;
        currentProcess = nextProcess;

        return nextProcess->savedStackPointer;
    }

    // Si el proceso anterior sigue listo, puede continuar.
    if (previousProcess->state == PROCESS_READY) {
        previousProcess->state = PROCESS_RUNNING;
        return savedRsp;
    }

    // El proceso anterior no puede seguir ejecutandose.
    // Si existe un contexto de shell, regresamos a el.
    if (shellSavedStackPointer != 0) {
        currentProcess = NULL;
        return shellSavedStackPointer;
    }

    // Error de estado: no existe ningun contexto ejecutable.
    // No debemos reanudar un proceso bloqueado o terminado.
    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
