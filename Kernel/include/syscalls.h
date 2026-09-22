#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <stdint.h>
#include <registers.h>

/*
 * Numeros de syscall.
*/
#define SYS_WRITE 1
#define SYS_READ 2
#define SYS_CLEAR     3
#define SYS_FONT_UP   4
#define SYS_FONT_DOWN 5
#define SYS_GET_REGS 6
#define SYS_GET_TICKS 7
#define SYS_DRAW_RECT  8
#define SYS_BEEP       9
#define SYS_GET_TIME   10
#define SYS_SET_HASH   11
#define SYS_GET_HASH   12
#define SYS_CRACK      13

/*
 * Convencion de argumentos:
 *
 * RAX = numero de syscall
 * RDI = primer argumento
 * RSI = segundo argumento
 * RDX = tercer argumento
 *
 * Retorno:
 * RAX = valor devuelto por la syscall
 */
uint64_t syscallDispatcher(RegisterFrame *frame);

#endif