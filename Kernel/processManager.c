#include <pipeClose.h>
#include <semaphore.h>
#include <pipe.h>
#include <ioDescriptor.h>
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
 * Creditos de planificacion por posicion de la tabla.
 * Un proceso con mayor prioridad recibe mas turnos.
 */
static uint64_t schedulingCredits[MAX_PROCESSES] = {0};

static uint64_t schedulingCreditOwner[MAX_PROCESSES] = {0};

static uint64_t schedulingWeight(uint64_t priority) {
    if (priority == 0) {
        return 1;
    }

    if (priority > 5) {
        return 5;
    }

    return priority;
}

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

/*
 * Cada reserva de Userland pertenece al proceso
 * que ejecuto SYS_MALLOC.
 */
typedef struct ProcessAllocation {
    void *ptr;
    struct ProcessAllocation *next;
} ProcessAllocation;

void *allocateProcessMemory(uint64_t size) {
    uint64_t flags = saveAndDisableInterrupts();
    Process *process = currentProcess;

    /*
     * La shell original todavia no tiene PCB.
     * Conservar su comportamiento anterior.
     */
    if (process == NULL) {
        void *ptr = allocateMemory(size);
        restoreInterrupts(flags);
        return ptr;
    }

    if (size == 0) {
        restoreInterrupts(flags);
        return NULL;
    }

    ProcessAllocation *record =
        allocateMemory(sizeof(ProcessAllocation));

    if (record == NULL) {
        restoreInterrupts(flags);
        return NULL;
    }

    void *ptr = allocateMemory(size);

    if (ptr == NULL) {
        freeMemory(record);
        restoreInterrupts(flags);
        return NULL;
    }

    record->ptr = ptr;
    record->next = process->allocations;
    process->allocations = record;

    restoreInterrupts(flags);
    return ptr;
}

void freeProcessMemory(void *ptr) {
    uint64_t flags = saveAndDisableInterrupts();
    Process *process = currentProcess;

    if (ptr == NULL) {
        restoreInterrupts(flags);
        return;
    }

    if (process == NULL) {
        freeMemory(ptr);
        restoreInterrupts(flags);
        return;
    }

    ProcessAllocation **link = &process->allocations;

    while (*link != NULL && (*link)->ptr != ptr) {
        link = &(*link)->next;
    }

    /*
     * No permitir que un proceso libere una
     * reserva perteneciente a otro.
     */
    if (*link != NULL) {
        ProcessAllocation *record = *link;
        *link = record->next;

        freeMemory(record->ptr);
        freeMemory(record);
    }

    restoreInterrupts(flags);
}

/*
 * Ejecutar solo cuando el proceso ya no
 * puede volver a utilizar sus reservas.
 */
static void releaseProcessMemory(Process *process) {
    ProcessAllocation *record = process->allocations;
    process->allocations = NULL;

    while (record != NULL) {
        ProcessAllocation *next = record->next;

        freeMemory(record->ptr);
        freeMemory(record);

        record = next;
    }
}

void initializeProcessManager(void) {
    initializeProcessTable();
    nextPid = 1;
    nextProcessIndex = 0;
    currentProcess = NULL;
    shellSavedStackPointer = 0;

    for (uint64_t i = 0; i < MAX_PROCESSES; i++) {
        schedulingCredits[i] = 0;
        schedulingCreditOwner[i] = 0;
    }
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
/*
 * Copia un descriptor del proceso creador al hijo.
 * Solo se utiliza antes de registrar al nuevo proceso.
 */
static uint8_t inheritProcessDescriptor(
    Process *child,
    IODescriptor *parentTable,
    uint64_t sourceFd,
    uint64_t targetFd,
    uint8_t requiredPermission
) {
    if (child == NULL ||
        parentTable == NULL ||
        sourceFd >= MAX_PROCESS_DESCRIPTORS ||
        targetFd >= MAX_PROCESS_DESCRIPTORS) {
        return 0;
    }

    IODescriptor descriptor;

    if (!ioGetDescriptor(parentTable, sourceFd, &descriptor) ||
        descriptor.permissions != requiredPermission) {
        return 0;
    }

    if (descriptor.type == IO_DESCRIPTOR_KEYBOARD) {
        if (requiredPermission != IO_ACCESS_READ) {
            return 0;
        }
    } else if (descriptor.type == IO_DESCRIPTOR_TERMINAL) {
        if (requiredPermission != IO_ACCESS_WRITE) {
            return 0;
        }
    } else if (descriptor.type != IO_DESCRIPTOR_PIPE) {
        return 0;
    }

    if (descriptor.type == IO_DESCRIPTOR_PIPE) {
        Pipe *pipe = findPipeById(descriptor.resourceId);

        if (pipe == NULL) {
            return 0;
        }

        if (requiredPermission == IO_ACCESS_READ) {
            pipe->readers++;
        } else {
            pipe->writers++;
        }
    }

    child->ioDescriptors[targetFd] = descriptor;
    return 1;
}

static uint64_t createProcessInternal(
    const char *name,
    void (*simpleEntry)(void),
    void (*argsEntry)(uint64_t, char **),
    uint64_t priority,
    uint64_t parentPid,
    uint8_t foreground,
    uint64_t argc,
    const char *const argv[],
    uint64_t stdinFd,
    uint64_t stdoutFd
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

    /*
     * Todos los procesos comienzan con entrada por
     * teclado y salida por terminal.
     *
     * El resto de los descriptores comienza libre
     * gracias a la inicializacion del PCB en cero.
     */
    process->ioDescriptors[IO_FD_STDIN] = (IODescriptor){
        .inUse = 1,
        .permissions = IO_ACCESS_READ,
        .type = IO_DESCRIPTOR_KEYBOARD,
        .resourceId = 0
    };

    process->ioDescriptors[IO_FD_STDOUT] = (IODescriptor){
        .inUse = 1,
        .permissions = IO_ACCESS_WRITE,
        .type = IO_DESCRIPTOR_TERMINAL,
        .resourceId = 0
    };

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

    /*
     * Configurar STDIN y STDOUT antes de registrar el proceso.
     * Hasta este punto el hijo no puede ser planificado.
     */
    uint64_t ioFlags = saveAndDisableInterrupts();
    IODescriptor *parentTable = ioGetCurrentDescriptorTable();

    uint8_t inputReady = inheritProcessDescriptor(
        process, parentTable, stdinFd, IO_FD_STDIN, IO_ACCESS_READ
    );

    uint8_t outputReady = 0;

    if (inputReady) {
        outputReady = inheritProcessDescriptor(
            process, parentTable, stdoutFd, IO_FD_STDOUT, IO_ACCESS_WRITE
        );
    }

    if (!inputReady || !outputReady) {
        pipeCloseAllDescriptors(process->ioDescriptors);
        restoreInterrupts(ioFlags);
        freeMemory(stack);
        freeMemory(process);
        return 0;
    }

    if (!registerProcess(process)) {
        pipeCloseAllDescriptors(process->ioDescriptors);
        restoreInterrupts(ioFlags);
        freeMemory(stack);
        freeMemory(process);
        return 0;
    }

    restoreInterrupts(ioFlags);
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
        NULL,
        IO_FD_STDIN,
        IO_FD_STDOUT
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
        argv,
        IO_FD_STDIN,
        IO_FD_STDOUT
    );
}

/*
 * Crea un proceso con STDIN y STDOUT configurados
 * antes de incorporarlo al scheduler.
 */
uint64_t createProcessWithRedirectedIO(
    const char *name,
    void (*entryPoint)(uint64_t argc, char **argv),
    uint64_t priority,
    uint64_t parentPid,
    uint8_t foreground,
    uint64_t argc,
    const char *const argv[],
    uint64_t stdinFd,
    uint64_t stdoutFd
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
        argv,
        stdinFd,
        stdoutFd
    );
}

Process *selectNextReadyProcess(void) {
    for (uint64_t checked = 0; checked < MAX_PROCESSES; checked++) {
        uint64_t index =
            (nextProcessIndex + checked) % MAX_PROCESSES;

        Process *process = getProcessAt(index);

        if (process == NULL || process->state != PROCESS_READY) {
            schedulingCredits[index] = 0;
            schedulingCreditOwner[index] = 0;
            continue;
        }

        /*
         * Si el slot ahora pertenece a otro PID,
         * descartar los creditos del proceso anterior.
         */
        if (schedulingCreditOwner[index] != process->pid) {
            schedulingCredits[index] = 0;
            schedulingCreditOwner[index] = process->pid;
        }

        if (schedulingCredits[index] == 0) {
            schedulingCredits[index] =
                schedulingWeight(process->priority);
        }

        schedulingCredits[index]--;

        if (schedulingCredits[index] == 0) {
            nextProcessIndex = (index + 1) % MAX_PROCESSES;
        } else {
            nextProcessIndex = index;
        }

        return process;
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

    /* Cerrar pipes antes de abandonar definitivamente la CPU. */
    pipeCleanupProcess(process);

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

    /*
    * Invalidar los creditos pendientes para que
    * el nuevo valor se aplique inmediatamente.
    */
    for (uint64_t i = 0; i < MAX_PROCESSES; i++) {
        if (getProcessAt(i) == process) {
            schedulingCredits[i] = 0;
            break;
        }
    }

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
        process->state == PROCESS_BLOCKED &&
        process->waitingSemaphoreId == 0 &&
        process->waitingPipeId == 0) {

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

        /*
         * Marcarlo como terminado antes de limpiar sus recursos:
         * ningun despertar debe volver a hacerlo ejecutable.
         */
        process->state = PROCESS_TERMINATED;
        pipeCleanupProcess(process);
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
         * En el kernel real, la shell recoge sus hijos foreground.
         * Los tests hosted conservan la semantica original del reaper.
         */
#if !__STDC_HOSTED__
        if (process->parentPid == 0 && process->foreground) {
            continue;
        }
#endif

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
        releaseProcessMemory(process);
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

    if (childPid == 0) {
        restoreInterrupts(flags);
        return 0;
    }

    Process *child = findProcessByPid(childPid);

    if (child == NULL ||
        child->parentPid != (parent != NULL ? parent->pid : 0)) {
        restoreInterrupts(flags);
        return 0;
    }

    /*
     * Mientras el hijo no termine, bloquear al padre
     * y ceder la CPU.
     */
    while (child->state != PROCESS_TERMINATED) {
        if (parent == NULL) {
#if __STDC_HOSTED__
            restoreInterrupts(flags);
            return 0;
#else
            /*
             * La shell no tiene PCB. Espera al siguiente
             * interrupt sin impedir el avance del scheduler.
             */
            restoreInterrupts(flags);
            return 0;
#endif
        } else {
            parent->waitingForChildPid = childPid;
            parent->state = PROCESS_BLOCKED;

#if __STDC_HOSTED__
            restoreInterrupts(flags);
            return 0;
#else
            __asm__ volatile ("int $0x81" ::: "memory");
#endif
        }
    }

    /*
     * El hijo termino y el padre ya puede recogerlo.
     */
    if (parent != NULL) {
        parent->waitingForChildPid = 0;
    }

    /*
     * Ctrl+C solo marca procesos como terminados.
     * Cerrar sus recursos ahora, fuera de IRQ1.
     * La limpieza es segura tambien tras un exit normal.
     */
    pipeCleanupProcess(child);
    semCancelWaiter(childPid);

    if (!unregisterProcess(childPid)) {
        restoreInterrupts(flags);
        return 0;
    }

    void *stack = child->stackBase;

    releaseProcessMemory(child);
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

/*
 * Desbloquea un proceso que esperaba este semaforo.
 *
 * El desbloqueo manual no puede realizar esta operacion.
 */
uint8_t unblockProcessFromSemaphore(
    uint64_t pid,
    uint64_t semaphoreId
) {
    if (pid == 0 || semaphoreId == 0) {
        return 0;
    }

    uint64_t flags = saveAndDisableInterrupts();
    Process *process = findProcessByPid(pid);

    if (process == NULL ||
        process->state != PROCESS_BLOCKED ||
        process->waitingSemaphoreId != semaphoreId) {
        restoreInterrupts(flags);
        return 0;
    }

    process->waitingSemaphoreId = 0;
    process->state = PROCESS_READY;

    restoreInterrupts(flags);
    return 1;
}


/*
 * Desbloqueo autorizado para una espera por pipe.
 *
 * Comprueba que el proceso este bloqueado esperando
 * exactamente el pipe y el modo indicados.
 */
uint8_t unblockProcessFromPipe(
    uint64_t pid,
    uint64_t pipeId,
    PipeMode mode
) {
    if (pid == 0 ||
        pipeId == 0 ||
        (mode != PIPE_READ && mode != PIPE_WRITE)) {
        return 0;
    }

    uint64_t flags = saveAndDisableInterrupts();

    Process *process = findProcessByPid(pid);

    if (process == NULL ||
        process->state != PROCESS_BLOCKED ||
        process->waitingSemaphoreId != 0 ||
        process->waitingPipeId != pipeId ||
        process->waitingPipeMode != mode) {

        restoreInterrupts(flags);
        return 0;
    }

    process->waitingPipeId = 0;
    process->waitingPipeMode = PIPE_READ;
    process->state = PROCESS_READY;

    restoreInterrupts(flags);
    return 1;
}
