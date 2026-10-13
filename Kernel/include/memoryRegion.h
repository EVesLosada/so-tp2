
#ifndef MEMORY_REGION_H
#define MEMORY_REGION_H

#include <stdint.h>

/*
 * Region contigua de memoria fisica.
 *
 * start: primera direccion administrable.
 * size: cantidad de bytes administrables.
 */
typedef struct {
    void *start;
    uint64_t size;
} MemoryRegion;

/*
 * Busca una region E820 utilizable por encima de reservedEnd.
 * reservedEnd es el limite superior exclusivo de la memoria
 * que ya esta ocupada por componentes del sistema.
 *
 * La region seleccionada debe estar alineada a PAGE_SIZE.
 *
 * Retorna:
 *   1 si encontro una region valida.
 *   0 si no encontro memoria segura o region es NULL.
 *
 * Si retorna 0 y region no es NULL, deja start en NULL
 * y size en 0.
 */
uint8_t selectMemoryRegion(uint64_t reservedEnd,
                           MemoryRegion *region);

#endif
