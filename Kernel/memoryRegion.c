#include <stdint.h>
#include <stddef.h>

#include <memoryRegion.h>
#include <memoryMap.h>
#include <memoryLayout.h>

/*
 * El mapa E820 empieza en 0x4000 y la zona de
 * informacion del bootloader empieza en 0x5000.
 *
 * Con entradas separadas por 32 bytes, hay espacio
 * para 128 registros antes de esa zona.
 */
#define E820_MAX_ENTRIES 128UL

/*
 * Redondea una direccion hacia arriba al
 * siguiente limite de pagina.
 *
 * Retorna 0 si el calculo desbordaria uint64_t.
 */
static uint8_t alignUpToPage(uint64_t address,
                             uint64_t *result)
{
    uint64_t remainder = address % PAGE_SIZE;
    uint64_t padding = remainder == 0
        ? 0
        : PAGE_SIZE - remainder;

    if (address > UINT64_MAX - padding) {
        return 0;
    }

    *result = address + padding;
    return 1;
}

/*
 * Redondea una direccion hacia abajo al
 * limite de pagina anterior.
 */
static uint64_t alignDownToPage(uint64_t address)
{
    return address - (address % PAGE_SIZE);
}

uint8_t selectMemoryRegion(uint64_t reservedEnd,
                           MemoryRegion *region)
{
    if (region == NULL) {
        return 0;
    }

    region->start = NULL;
    region->size = 0;

    for (uint64_t i = 0; i < E820_MAX_ENTRIES; i++) {
        const E820Entry *entry = getE820Entry(i);

        if (isE820End(entry)) {
            break;
        }

        if (entry->type != E820_TYPE_USABLE ||
            entry->length == 0) {
            continue;
        }

        /*
         * Evitar overflow al calcular el final.
         */
        if (entry->baseAddress >
            UINT64_MAX - entry->length) {
            continue;
        }

        uint64_t start = entry->baseAddress;
        uint64_t end =
            entry->baseAddress + entry->length;

        /*
         * Excluir toda la memoria reservada
         * por debajo de reservedEnd.
         */
        if (start < reservedEnd) {
            start = reservedEnd;
        }

        if (!alignUpToPage(start, &start)) {
            continue;
        }

        end = alignDownToPage(end);

        if (start >= end) {
            continue;
        }

        uint64_t size = end - start;

        /*
         * Conservar la region utilizable mas grande.
         */
        if (size > region->size) {
            region->start = (void *)start;
            region->size = size;
        }
    }

    return region->size != 0;
}
