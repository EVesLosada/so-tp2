#include <stdint.h>
#include <stddef.h>

#include <processManager.h>
#include <processTable.h>
#include <memoryManager.h>
#include <registers.h>

#define KERNEL_CODE_SELECTOR 0x08ULL
#define INITIAL_RFLAGS 0x202ULL
#define STACK_ALIGNMENT 16ULL

static uint64_t nextPid = 1;

/*
 * Punto de arranque para un proceso nuevo.
 *
 * El futuro context switch comenzara la ejecucion aqui.
 */
static void processBootstrap(Process *process) {
    process->entryPoint();

    /*
     * La liberacion definitiva de recursos se implementara
     * en el Bloque 10.
     */
    process->state = PROCESS_TERMINATED;

    for (;;) {
        __asm__ volatile ("hlt");
    }
}

/*
 * Inicializar la tabla y la numeracion de procesos.
 */
void initializeProcessManager(void) {
    initializeProcessTable();
    nextPid = 1;
}

/*
 * Crear un PCB, reservar su stack y preparar
 * el contexto inicial de ejecucion.
 */
uint64_t createProcess(const char *name,
                       void (*entryPoint)(void),
                       uint64_t priority,
                       uint64_t parentPid,
                       uint8_t foreground) {

    if (name == NULL ||
        name[0] == '\0' ||
        entryPoint == NULL ||
        nextPid == UINT64_MAX ||
        getProcessCount() >= MAX_PROCESSES) {
        return 0;
    }

    /*
     * Reservar memoria para el PCB.
     */
    Process *process = allocateMemory(sizeof(Process));

    if (process == NULL) {
        return 0;
    }

    /*
     * Cada proceso tiene su propio stack.
     */
    void *stack = allocateMemory(PROCESS_STACK_SIZE);

    if (stack == NULL) {
        freeMemory(process);
        return 0;
    }

    /*
     * Inicializar el PCB.
     */
    *process = (Process){0};

    process->pid = nextPid;
    process->priority = priority;
    process->parentPid = parentPid;
    process->foreground = foreground != 0;
    process->state = PROCESS_READY;

    process->stackBase = stack;
    process->stackSize = PROCESS_STACK_SIZE;
    process->entryPoint = entryPoint;

    /*
     * Copiar el nombre sin desbordar el buffer.
     */
    uint64_t i = 0;

    while (i < PROCESS_NAME_MAX_LENGTH - 1 &&
           name[i] != '\0') {
        process->name[i] = name[i];
        i++;
    }

    process->name[i] = '\0';

    /*
     * Preparar el stack inicial.
     *
     * El contexto tiene el formato de RegisterFrame,
     * compatible con saveState/restoreState del kernel.
     */
    uintptr_t top =
        ((uintptr_t)stack + PROCESS_STACK_SIZE) &
        ~(uintptr_t)(STACK_ALIGNMENT - 1);

    uintptr_t entryRsp = top - sizeof(uint64_t);

    /*
     * Direccion de retorno de seguridad.
     * processBootstrap nunca retorna.
     */
    *(uint64_t *)entryRsp = 0;

    RegisterFrame *frame =
        (RegisterFrame *)(entryRsp - sizeof(RegisterFrame));

    *frame = (RegisterFrame){0};

    frame->rsp = entryRsp;
    frame->rip = (uint64_t)(uintptr_t)processBootstrap;
    frame->cs = KERNEL_CODE_SELECTOR;
    frame->rflags = INITIAL_RFLAGS;
    frame->rdi = (uint64_t)(uintptr_t)process;

    process->savedStackPointer = (uint64_t)(uintptr_t)frame;

    /*
     * Registrar el proceso en la tabla existente.
     */
    if (!registerProcess(process)) {
        freeMemory(stack);
        freeMemory(process);
        return 0;
    }

    /*
     * El siguiente proceso recibira otro PID.
     */
    return nextPid++;
}