
#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <syscall_types.h>

#define MAX_PROCESSES 64
#define PROCESS_STACK_SIZE (16UL * 1024UL)

/*
 * PCB (Process Control Block).
 *
 * Representa un proceso dentro del kernel.
 * No debe confundirse con ProcessInfo, que es
 * la estructura publica utilizada por Userland.
 */
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
     * Indica si el proceso ocupa el foreground.
     */
    uint8_t foreground;

} Process;

#endif
