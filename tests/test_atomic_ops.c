#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <atomicOps.h>

static void testExchange(void) {
    volatile uint64_t value = 10;

    assert(atomicExchange64(&value, 20) == 10);
    assert(value == 20);

    assert(atomicExchange64(&value, 0) == 20);
    assert(value == 0);

    assert(atomicExchange64(&value, UINT64_MAX) == 0);
    assert(value == UINT64_MAX);
}

static void testCompareExchange(void) {
    volatile uint64_t value = 15;

    /* Comparacion correcta: actualiza el valor. */
    assert(atomicCompareExchange64(&value, 15, 30) == 1);
    assert(value == 30);

    /* Comparacion incorrecta: no modifica memoria. */
    assert(atomicCompareExchange64(&value, 15, 50) == 0);
    assert(value == 30);

    /* Tambien permite intercambiar valores extremos. */
    assert(atomicCompareExchange64(&value, 30, UINT64_MAX) == 1);
    assert(value == UINT64_MAX);

    assert(atomicCompareExchange64(&value, UINT64_MAX, 0) == 1);
    assert(value == 0);
}

int main(void) {
    testExchange();
    testCompareExchange();

    printf("OK: primitivas atomicas x86-64 superadas\n");
    return 0;
}
