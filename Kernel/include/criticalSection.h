#ifndef CRITICAL_SECTION_H
#define CRITICAL_SECTION_H

#include <stdint.h>

/*
 * Proteccion de secciones criticas en un unico CPU.
 *
 * Guarda RFLAGS y deshabilita interrupciones.
 * Devuelve las flags anteriores para restaurarlas.
 */
static inline uint64_t enterCriticalSection(void) {
    uint64_t flags;

    __asm__ volatile (
        "pushfq\n\t"
        "popq %0\n\t"
        "cli"
        : "=r"(flags)
        :
        : "memory", "cc"
    );

    return flags;
}

/*
 * Restaura RFLAGS, incluido el estado previo de IF.
 * Esto permite anidar secciones criticas correctamente.
 */
static inline void leaveCriticalSection(uint64_t flags) {
    __asm__ volatile (
        "pushq %0\n\t"
        "popfq"
        :
        : "r"(flags)
        : "memory", "cc"
    );
}

#endif
