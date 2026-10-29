#ifndef ATOMIC_OPS_H
#define ATOMIC_OPS_H

#include <stdint.h>

/*
 * Intercambia atomica y simultaneamente el valor
 * almacenado en address por newValue.
 *
 * Retorna el valor anterior.
 *
 * En x86-64, XCHG con un operando de memoria
 * tiene atomicidad implicita.
 */
static inline uint64_t atomicExchange64(
    volatile uint64_t *address,
    uint64_t newValue
) {
    __asm__ volatile (
        "xchgq %0, %1"
        : "+r"(newValue), "+m"(*address)
        :
        : "memory"
    );

    return newValue;
}

/*
 * Reemplaza *address por desired solamente si
 * su valor actual es igual a expected.
 *
 * Retorna 1 si el intercambio se realizo.
 * Retorna 0 si el valor no coincidia.
 *
 * LOCK CMPXCHG garantiza la atomicidad
 * de la comparacion y la escritura.
 */
static inline uint8_t atomicCompareExchange64(
    volatile uint64_t *address,
    uint64_t expected,
    uint64_t desired
) {
    uint64_t observed = expected;

    __asm__ volatile (
        "lock cmpxchgq %2, %1"
        : "+a"(observed), "+m"(*address)
        : "r"(desired)
        : "memory", "cc"
    );

    return observed == expected;
}

#endif
