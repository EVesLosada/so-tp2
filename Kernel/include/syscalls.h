
#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <stdint.h>
#include <registers.h>
#include <syscall_ids.h>
#include <syscall_types.h>

/*
 * Convencion de system calls:
 *
 * RAX = identificador de syscall
 * RDI = primer argumento
 * RSI = segundo argumento
 * RDX = tercer argumento
 * RCX = cuarto argumento
 * R8  = quinto argumento
 *
 * Retorno:
 * RAX = valor devuelto por la syscall
 *
 * Los punteros recibidos desde Userland solo se utilizan
 * para intercambiar datos a traves de la interfaz de syscalls.
 */
uint64_t syscallDispatcher(RegisterFrame *frame);

#endif
