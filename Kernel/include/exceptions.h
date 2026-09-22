#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <stdint.h>
#include <registers.h>

#define DIVIDE_BY_ZERO_EXCEPTION 0
#define INVALID_OPCODE_EXCEPTION 6


/*
 * Atiende una excepcion capturada por un handler ASM.
 *
 * Imprime informacion de debug:
 * - tipo de excepcion
 * - registros generales
 * - RIP, que indica la direccion de la instruccion que fallo.
 */
void exceptionDispatcher(uint64_t exception, RegisterFrame *frame);

#endif