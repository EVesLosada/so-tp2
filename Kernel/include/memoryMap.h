#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

#include <stdint.h>

/*
 * Pure64 deja el mapa E820 a partir de la direccion 0x4000.
 *
 * Cada entrada contiene 24 bytes de informacion, pero
 * Pure64 reserva 32 bytes por entrada.
 */
#define E820_MAP_ADDRESS 0x4000UL
#define E820_ENTRY_STRIDE 32UL

#define E820_TYPE_USABLE 1U

typedef struct __attribute__((packed)) {
    uint64_t baseAddress;
    uint64_t length;
    uint32_t type;
    uint32_t extendedAttributes;
} E820Entry;

/*
 * Devuelve la entrada E820 ubicada en el indice indicado.
 */
const E820Entry *getE820Entry(uint64_t index);

/*
 * Indica si una entrada marca el final del mapa E820.
 */
uint8_t isE820End(const E820Entry *entry);

#endif