#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <stdint.h>
#include <syscall_types.h>

/*
 * Interfaz comun de los administradores de memoria.
 *
 * Tanto Free List como Buddy System deben implementar
 * estas operaciones. De esta forma, el resto del kernel
 * no depende del algoritmo utilizado.
 */

/*
 * Inicializa el administrador sobre una region de memoria.
 */
void initializeMemoryManager(void *memoryStart, uint64_t memorySize);

/*
 * Reserva un bloque de al menos 'size' bytes.
 * Retorna NULL si no hay memoria suficiente.
 */
void *allocateMemory(uint64_t size);

/*
 * Libera un bloque previamente reservado.
 */
void freeMemory(void *ptr);

/*
 * Completa la estructura con el estado actual
 * de la memoria administrada.
 */
void getMemoryInfo(MemoryInfo *info);

#endif