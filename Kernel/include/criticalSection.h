#ifndef CRITICAL_SECTION_H
#define CRITICAL_SECTION_H

#include <stdint.h>

/*
 * Proteccion de secciones criticas en un unico CPU.
 *
 * Guarda RFLAGS y deshabilita interrupciones.
 * Devuelve las flags anteriores para restaurarlas.
 *
 * En los tests hosted de Linux no podemos ejecutar CLI.
 */
static inline uint64_t enterCriticalSection(void) {
#if defined(__STDC_HOSTED__) && __STDC_HOSTED__
    return 0;
#else
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
#endif
}

/*
 * Restaura RFLAGS, incluido el estado previo de IF.
 * Esto permite anidar secciones criticas correctamente.
 */
static inline void leaveCriticalSection(uint64_t flags) {
#if defined(__STDC_HOSTED__) && __STDC_HOSTED__
    (void)flags;
#else
    __asm__ volatile (
        "pushq %0\n\t"
        "popfq"
        :
        : "r"(flags)
        : "memory", "cc"
    );
#endif
}

#endif
