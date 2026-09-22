#ifndef REGISTERS_H
#define REGISTERS_H

#include <stdint.h>

/*
 * Estado guardado al entrar a una interrupcion de software.
 *
 * El orden debe coincidir con saveState en Kernel/asm/libasm.asm.
 *
 * Para el modelo actual del TP, el modulo Userland se ejecuta
 * como codigo cargado por el kernel, sin cambio de nivel de privilegio.
 * Por eso la CPU apila RIP, CS y RFLAGS al ejecutar int 80h.
 */
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

/*
 * Guarda una copia del estado actual de los registros.
 *
 * Se llama desde la interrupcion de teclado cuando se presiona F1.
 * El snapshot queda almacenado en kernel para poder consultarlo despues
 * desde Userland mediante la syscall SYS_GET_REGS.
 */
void saveRegistersSnapshot(RegisterFrame *frame);

/*
 * Copia el ultimo snapshot guardado en destination.
 *
 * Retorna:
 *   1: si habia un snapshot disponible
 *   0: si todavia no se guardo ninguno
 */
uint8_t getRegistersSnapshot(RegisterFrame *destination);

#endif