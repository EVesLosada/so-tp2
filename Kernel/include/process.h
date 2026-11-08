
#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <syscall_types.h>
#include <ioDescriptor.h>

#define MAX_PROCESSES 64
#define PROCESS_STACK_SIZE (16UL * 1024UL)

/*
 * PCB (Process Control Block).
 *
 * Representa un proceso dentro del kernel.
 * No debe confundirse con ProcessInfo, que es
 * la estructura publica utilizada por Userland.
 */
struct ProcessAllocation;

typedef struct Process {
    uint64_t pid;
    char name[PROCESS_NAME_MAX_LENGTH];

    ProcessState state;
    uint64_t priority;

    /*
     * Contexto de ejecucion.
     *
     * savedStackPointer apunta al contexto guardado
     * que utilizara el futuro cambio de contexto.
     */
    uint64_t savedStackPointer;

    /*
     * Limites del stack asignado al proceso.
     */
    void *stackBase;
    uint64_t stackSize;

    /*
     * Punto de entrada del proceso.
     */
    void (*entryPoint)(void);

    /*
    * Punto de entrada para procesos con argumentos.
    *
    * argv apuntara a una copia de los argumentos
    * almacenada en memoria propia del proceso.
    */
    void (*entryPointWithArgs)(uint64_t argc, char **argv);
    uint64_t argc;
    char **argv;

    /*
     * Relacion con otros procesos.
     */
    uint64_t parentPid;

    /*
     * PID del hijo cuya terminacion espera este proceso.
     * 0 indica que no esta esperando a ningun hijo.
     */
    uint64_t waitingForChildPid;

    /* 0 si no espera un semaforo. */
    uint64_t waitingSemaphoreId;

    /*
     * Pipe que esta esperando, o 0 si no espera ninguno.
     * El modo indica si espera datos o espacio disponible.
     */
    uint64_t waitingPipeId;
    PipeMode waitingPipeMode;

    /*
     * Indica si el proceso ocupa el foreground.
     */
    uint8_t foreground;

    /*
     * Tabla de descriptores de entrada/salida.
     * Cada proceso posee sus propias entradas.
     */
    IODescriptor ioDescriptors[MAX_PROCESS_DESCRIPTORS];

    /* Reservas dinamicas propiedad de este proceso. */
    struct ProcessAllocation *allocations;

} Process;

#endif
