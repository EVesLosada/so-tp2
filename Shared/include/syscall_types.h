#ifndef SYSCALL_TYPES_H
#define SYSCALL_TYPES_H

#include <stdint.h>

#define PROCESS_NAME_MAX_LENGTH 32

/*
 * Informacion publica del administrador de memoria.
 * Se utiliza para implementar el comando mem.
 */
typedef struct {
    uint64_t totalMemory;
    uint64_t usedMemory;
    uint64_t freeMemory;
} MemoryInfo;

/*
 * Estados de proceso visibles desde Userland.
 */
typedef enum {
    PROCESS_READY = 0,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_TERMINATED
} ProcessState;

/*
 * Informacion publica de un proceso.
 * Se utiliza para implementar ps.
 */
typedef struct {
    uint64_t pid;
    char name[PROCESS_NAME_MAX_LENGTH];

    uint64_t priority;
    ProcessState state;

    uint64_t stackPointer;
    uint64_t basePointer;

    uint8_t foreground;
} ProcessInfo;

/*
 * Modo de apertura de un extremo de pipe.
 */
typedef enum {
    PIPE_READ = 0,
    PIPE_WRITE = 1
} PipeMode;

/*
 * Parametros para crear un proceso con STDIN y STDOUT
 * configurados antes de incorporarlo al scheduler.
 */
typedef struct {
    const char *name;
    void (*entryPoint)(uint64_t argc, char **argv);
    uint64_t priority;
    uint8_t foreground;
    uint64_t argc;
    const char *const *argv;
    uint64_t stdinFd;
    uint64_t stdoutFd;
} ProcessCreateIOArgs;

#endif