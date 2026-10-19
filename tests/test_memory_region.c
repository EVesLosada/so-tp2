#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <memoryMap.h>
#include <memoryRegion.h>

static E820Entry entries[128];

const E820Entry *getE820Entry(uint64_t index) {
    assert(index < 128);
    return &entries[index];
}

uint8_t isE820End(const E820Entry *entry) {
    return entry == NULL ||
        (entry->baseAddress == 0 &&
         entry->length == 0 &&
         entry->type == 0);
}

static void resetMap(void) {
    memset(entries, 0, sizeof(entries));
}

static void put(uint64_t index, uint64_t base,
                uint64_t length, uint32_t type) {
    entries[index] = (E820Entry){base, length, type, 1};
}

static void expect(uint64_t reservedEnd, uint8_t ok,
                   uint64_t expectedStart,
                   uint64_t expectedSize) {
    MemoryRegion region = {(void *)0x1234, 1234};

    assert(selectMemoryRegion(reservedEnd, &region) == ok);
    assert((uintptr_t)region.start == expectedStart);
    assert(region.size == expectedSize);
}

int main(void) {
    /* Mapa observado en QEMU con 512 MiB. */
    resetMap();
    put(0, 0, 0x9FC00, 1);
    put(1, 0x9FC00, 0x400, 2);
    put(2, 0xF0000, 0x10000, 2);
    put(3, 0x100000, 0x1FEE0000, 1);
    put(4, 0x1FFE0000, 0x20000, 2);
    expect(0x600000, 1, 0x600000, 0x1F9E0000);

    /* Alineacion a paginas completas. */
    resetMap();
    put(0, 0x1003, 0x2FFF, 1);
    expect(0x2801, 1, 0x3000, 0x1000);

    /* Ignorar zonas reservadas; elegir la mayor. */
    resetMap();
    put(0, 0x1000, 0x5000, 2);
    put(1, 0x8000, 0x3000, 1);
    put(2, 0x10000, 0x5000, 1);
    expect(0, 1, 0x10000, 0x5000);

    /* Memoria insuficiente. */
    expect(0x20000, 0, 0, 0);

    /* Desbordamiento de direcciones. */
    resetMap();
    put(0, UINT64_MAX - 0x1000, 0x2000, 1);
    put(1, 0x3000, 0x3000, 1);
    expect(0, 1, 0x3000, 0x3000);

    /* Mapa vacio y salida invalida. */
    resetMap();
    expect(0, 0, 0, 0);
    assert(selectMemoryRegion(0, NULL) == 0);

    puts("OK: 6 pruebas del selector de memoria");
    return 0;
}
