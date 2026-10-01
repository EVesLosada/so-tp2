
#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <stdint.h>
#include <registers.h>
#include <syscall_ids.h>

/*
 * Convencion de argumentos:
 *
 * RAX = numero de syscall
 * RDI = primer argumento
 * RSI = segundo argumento
 * RDX = tercer argumento
 * RCX = cuarto argumento
 * R8  = quinto argumento
 *
 * Retorno:
 * RAX = valor devuelto por la syscall
 */
uint64_t syscallDispatcher(RegisterFrame *frame);

#endif
