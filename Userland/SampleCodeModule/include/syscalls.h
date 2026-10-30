#ifndef USERLAND_SYSCALLS_H
#define USERLAND_SYSCALLS_H

#include <stdint.h>
#include <syscall_ids.h>
#include <syscall_types.h>

#define STDIN  0
#define STDOUT 1

typedef struct {
    uint64_t rsp;

    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;

    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
} RegisterFrame;

uint64_t sysGetRegisters(RegisterFrame *registers);

uint64_t sysWrite(uint64_t fd, const char *buffer, uint64_t count);
uint64_t sysRead(uint64_t fd, char *buffer, uint64_t count);

uint64_t sysClear(void);
uint64_t sysFontUp(void);
uint64_t sysFontDown(void);

uint64_t sysGetTicks(void);
uint64_t sysDrawRect(uint32_t color, uint64_t x, uint64_t y, uint64_t width, uint64_t height);
uint64_t sysBeep(uint32_t frequency, uint64_t durationTicks);

uint64_t sysGetTime(uint8_t *hours, uint8_t *minutes, uint8_t *seconds);

uint64_t sysSetHash(uint64_t hash);
uint64_t sysGetHash(uint64_t *hash);

uint64_t sysCrack(char *resultBuffer,
                  uint64_t resultBufferSize,
                  uint64_t *elapsedCycles);

void *sysMalloc(uint64_t size);
void sysFree(void *ptr);
uint64_t sysMemInfo(MemoryInfo *info);

/*
 * Modifica la prioridad de un proceso.
 * Retorna 1 si tuvo exito, 0 si fallo.
 */
/*
 * Crea un proceso con argumentos.
 * Retorna el nuevo PID o 0 si falla.
 */
uint64_t sysCreateProcess(
    const char *name,
    void (*entryPoint)(uint64_t argc, char **argv),
    uint64_t priority,
    uint8_t foreground,
    uint64_t argc,
    const char *const argv[]
);

/*
 * Finaliza el proceso actual.
 * No retorna si tiene exito.
 * Devuelve 0 si no existe un proceso actual.
 */
uint64_t sysExit(void);

/*
 * Termina el proceso identificado por pid.
 * Retorna 1 si tuvo exito y 0 si fallo.
 * No retorna si el proceso se mata a si mismo.
 */
uint64_t sysKill(uint64_t pid);

/*
 * Espera la finalizacion de un hijo directo.
 * Retorna el PID del hijo cuando termina.
 * Retorna 0 si el PID es invalido o no es su hijo.
 */
uint64_t sysWait(uint64_t childPid);

uint64_t sysGetPid(void);

/* Retorna la cantidad de procesos copiados al buffer. */
uint64_t sysPs(ProcessInfo *buffer, uint64_t capacity);

uint64_t sysNice(uint64_t pid, uint64_t priority);

/* Bloquea un proceso por PID. Retorna 1 si tuvo exito. */
uint64_t sysBlock(uint64_t pid);

/* Desbloquea un proceso por PID. Retorna 1 si tuvo exito. */
uint64_t sysUnblock(uint64_t pid);

/* Cede voluntariamente la CPU. Retorna 1 al reanudarse. */
uint64_t sysYield(void);

/* Administracion de semaforos. */
uint64_t sysSemCreate(const char *name, uint64_t initialValue);
uint64_t sysSemOpen(const char *name);
uint64_t sysSemClose(uint64_t id);

#endif
