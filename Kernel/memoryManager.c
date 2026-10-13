#include <stdint.h>
#include <stddef.h>

#include <memoryManager.h>

/*
 * Administrador de memoria Free List.
 *
 * Los bloques libres se organizan en una lista
 * enlazada ordenada por direccion de memoria.
 */

typedef struct FreeBlock {
    uint64_t size;
    struct FreeBlock *next;
} FreeBlock;

/*
 * Estado global del administrador.
 */
static FreeBlock *freeList = NULL;
static uint64_t totalMemory = 0;

#define MEMORY_ALIGNMENT 8UL

/*
 * Redondea un tamaño hacia arriba a un multiplo de 8.
 * Retorna 0 si el calculo desbordaria uint64_t.
 */
static uint64_t alignSize(uint64_t size)
{
    uint64_t remainder = size % MEMORY_ALIGNMENT;

    if (remainder == 0) {
        return size;
    }

    uint64_t padding = MEMORY_ALIGNMENT - remainder;

    if (size > UINT64_MAX - padding) {
        return 0;
    }

    return size + padding;
}

/*
 * Inicializa la lista con un unico bloque libre
 * que ocupa toda la region administrable.
 */
void initializeMemoryManager(void *memoryStart, uint64_t memorySize)
{
    freeList = NULL;
    totalMemory = 0;

    if (memoryStart == NULL ||
        memorySize < sizeof(FreeBlock)) {
        return;
    }

    /*
     * La cabecera debe comenzar en una direccion
     * alineada correctamente.
     */
    if ((uintptr_t)memoryStart % sizeof(uint64_t) != 0) {
        return;
    }

    /*
    * Utilizar solamente bytes completos de alineacion.
    */
    memorySize -= memorySize % MEMORY_ALIGNMENT;

    if (memorySize < sizeof(FreeBlock)) {
        return;
    }

    freeList = (FreeBlock *)memoryStart;
    freeList->size = memorySize;
    freeList->next = NULL;

    totalMemory = memorySize;
}

/*
 * Reserva memoria utilizando First Fit.
 */
void *allocateMemory(uint64_t size)
{
    if (size == 0 || freeList == NULL) {
        return NULL;
    }

    uint64_t alignedSize = alignSize(size);

    if (alignedSize == 0 ||
        alignedSize > UINT64_MAX - sizeof(FreeBlock)) {
        return NULL;
    }

    uint64_t requiredSize = alignedSize + sizeof(FreeBlock);

    FreeBlock *current = freeList;
    FreeBlock *previous = NULL;

    while (current != NULL) {
        if (current->size >= requiredSize) {
            uint64_t remainingSize =
                current->size - requiredSize;

            if (remainingSize >=
                sizeof(FreeBlock) + MEMORY_ALIGNMENT) {

                /*
                 * Dividir el bloque:
                 * la primera parte queda reservada y
                 * la segunda permanece en la lista libre.
                 */
                FreeBlock *remaining =
                    (FreeBlock *)((uint8_t *)current + requiredSize);

                remaining->size = remainingSize;
                remaining->next = current->next;

                if (previous == NULL) {
                    freeList = remaining;
                } else {
                    previous->next = remaining;
                }

                current->size = requiredSize;
            } else {
                /*
                 * Si no alcanza para formar otro bloque,
                 * reservar el bloque completo.
                 */
                if (previous == NULL) {
                    freeList = current->next;
                } else {
                    previous->next = current->next;
                }
            }

            /*
             * Retornar la direccion inmediatamente
             * posterior a la cabecera.
             */
            return (void *)((uint8_t *)current +
                            sizeof(FreeBlock));
        }

        previous = current;
        current = current->next;
    }

    return NULL;
}

/*
 * Libera un bloque previamente reservado y
 * fusiona los bloques libres contiguos.
 */
void freeMemory(void *ptr)
{
    if (ptr == NULL || totalMemory == 0) {
        return;
    }

    FreeBlock *block =
        (FreeBlock *)((uint8_t *)ptr - sizeof(FreeBlock));

    FreeBlock *current = freeList;
    FreeBlock *previous = NULL;

    /*
     * Buscar la posicion correcta para mantener
     * la lista ordenada por direccion.
     */
    while (current != NULL &&
           (uintptr_t)current < (uintptr_t)block) {
        previous = current;
        current = current->next;
    }

    block->next = current;

    if (previous == NULL) {
        freeList = block;
    } else {
        previous->next = block;
    }

    /*
     * Fusionar con el siguiente bloque libre,
     * si ambos son contiguos.
     */
    if (current != NULL &&
        (uintptr_t)block + block->size ==
        (uintptr_t)current) {
        block->size += current->size;
        block->next = current->next;
    }

    /*
     * Fusionar con el bloque libre anterior.
     */
    if (previous != NULL &&
        (uintptr_t)previous + previous->size ==
        (uintptr_t)block) {
        previous->size += block->size;
        previous->next = block->next;
    }
}

/*
 * Informa el estado actual de la memoria administrada.
 */
void getMemoryInfo(MemoryInfo *info)
{
    if (info == NULL) {
        return;
    }

    uint64_t freeBytes = 0;
    FreeBlock *current = freeList;

    while (current != NULL) {
        freeBytes += current->size;
        current = current->next;
    }

    info->totalMemory = totalMemory;
    info->freeMemory = freeBytes;
    info->usedMemory = totalMemory - freeBytes;
}
