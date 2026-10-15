#include <stdint.h>
#include <stddef.h>

#include <memoryManager.h>

/*
 * Administrador de memoria Buddy System.
 *
 * Los bloques tienen tamaños que son potencias de 2.
 * Cada orden posee una lista de bloques libres.
 */

#define BUDDY_MIN_ORDER 5U
#define BUDDY_MAX_ORDER 63U
#define BUDDY_MIN_BLOCK_SIZE (1ULL << BUDDY_MIN_ORDER)
/*
 * Cabecera ubicada al comienzo de cada bloque.
 */
typedef struct BuddyBlock {
    struct BuddyBlock *next;
    uint8_t order;
} BuddyBlock;

/*
 * Una lista de bloques libres por cada orden.
 */
static BuddyBlock *freeLists[BUDDY_MAX_ORDER + 1];

/*
 * Region fisica administrada por Buddy.
 */
static uintptr_t poolStart;
static uint64_t poolSize;
/*
 * Inicializa Buddy sobre la region fisica disponible.
 */
void initializeMemoryManager(void *memoryStart, uint64_t memorySize)
{
    for (uint32_t i = 0; i <= BUDDY_MAX_ORDER; i++) {
        freeLists[i] = NULL;
    }

    poolStart = 0;
    poolSize = 0;

    if (memoryStart == NULL) {
        return;
    }

    uintptr_t start = (uintptr_t)memoryStart;

    /*
     * Alinear el comienzo de la region a 32 bytes.
     */
    uint64_t misalignment = start % BUDDY_MIN_BLOCK_SIZE;

    uint64_t padding = misalignment == 0
        ? 0
        : BUDDY_MIN_BLOCK_SIZE - misalignment;

    if (memorySize <= padding ||
        start > UINTPTR_MAX - padding) {
        return;
    }

    uintptr_t alignedStart = start + padding;

    uint64_t usableSize = memorySize - padding;
    usableSize -= usableSize % BUDDY_MIN_BLOCK_SIZE;

    if (usableSize < BUDDY_MIN_BLOCK_SIZE ||
        alignedStart > UINTPTR_MAX - usableSize) {
        return;
    }

    poolStart = alignedStart;
    poolSize = usableSize;

    /*
     * Dividir la region en bloques de potencia de 2.
     */
    uint64_t offset = 0;

    while (offset < poolSize) {
        uint64_t remaining = poolSize - offset;
        uint8_t order = BUDDY_MAX_ORDER;

        /*
         * Buscar el mayor bloque que entre en la region
         * y este correctamente alineado respecto del inicio.
         */
        while ((1ULL << order) > remaining ||
               offset % (1ULL << order) != 0) {
            order--;
        }

        BuddyBlock *block =
            (BuddyBlock *)(poolStart + offset);

        block->order = order;
        block->next = freeLists[order];
        freeLists[order] = block;

        offset += (1ULL << order);
    }
}
/*
 * Reserva memoria utilizando Buddy System.
 */
void *allocateMemory(uint64_t size)
{
    if (size == 0 || poolSize == 0) {
        return NULL;
    }

    /*
     * La reserva tambien necesita espacio para la cabecera.
     * Evitar desbordamiento en la suma.
     */
    if (size > UINT64_MAX - sizeof(BuddyBlock)) {
        return NULL;
    }

    uint64_t requiredSize = size + sizeof(BuddyBlock);

    /*
     * Buscar el menor orden que pueda contener
     * la cantidad de bytes solicitada.
     */
    uint8_t requestedOrder = BUDDY_MIN_ORDER;

    while (requestedOrder < BUDDY_MAX_ORDER &&
           (1ULL << requestedOrder) < requiredSize) {
        requestedOrder++;
    }

    if ((1ULL << requestedOrder) < requiredSize) {
        return NULL;
    }

    /*
     * Buscar un bloque libre de ese orden o mayor.
     */
    uint8_t currentOrder = requestedOrder;

    while (currentOrder <= BUDDY_MAX_ORDER &&
           freeLists[currentOrder] == NULL) {
        currentOrder++;
    }

    if (currentOrder > BUDDY_MAX_ORDER) {
        return NULL;
    }

    /*
     * Retirar el bloque encontrado de su lista libre.
     */
    BuddyBlock *block = freeLists[currentOrder];
    freeLists[currentOrder] = block->next;

    /*
     * Dividir el bloque hasta alcanzar el orden solicitado.
     * La mitad izquierda se conserva para la reserva.
     * La mitad derecha vuelve a la lista libre.
     */
    while (currentOrder > requestedOrder) {
        currentOrder--;

        BuddyBlock *buddy =
            (BuddyBlock *)((uint8_t *)block +
                           (1ULL << currentOrder));

        buddy->order = currentOrder;
        buddy->next = freeLists[currentOrder];
        freeLists[currentOrder] = buddy;
    }

    /*
     * Registrar el orden del bloque reservado.
     */
    block->order = requestedOrder;
    block->next = NULL;

    /*
     * Retornar la direccion posterior a la cabecera.
     */
    return (void *)((uint8_t *)block + sizeof(BuddyBlock));
}
/*
 * Libera un bloque y fusiona compañeros libres.
 */
void freeMemory(void *ptr)
{
    if (ptr == NULL || poolSize == 0) {
        return;
    }

    uintptr_t address = (uintptr_t)ptr;

    /*
     * Comprobar que la direccion pertenezca
     * a la region administrada.
     */
    if (address < poolStart + sizeof(BuddyBlock) ||
        address >= poolStart + poolSize) {
        return;
    }

    /*
     * Recuperar la cabecera del bloque reservado.
     */
    uintptr_t blockAddress =
        address - sizeof(BuddyBlock);

    uint64_t offset = blockAddress - poolStart;

    BuddyBlock *block = (BuddyBlock *)blockAddress;
    uint8_t order = block->order;

    if (order < BUDDY_MIN_ORDER ||
        order > BUDDY_MAX_ORDER) {
        return;
    }

    uint64_t blockSize = 1ULL << order;

    if (offset % blockSize != 0 ||
        blockSize > poolSize - offset) {
        return;
    }

    /*
     * Intentar fusionar mientras el compañero
     * del mismo orden este libre.
     */
    while (order < BUDDY_MAX_ORDER) {
        uint64_t buddyOffset =
            offset ^ (1ULL << order);

        /*
         * El compañero debe estar completamente
         * dentro de la region administrada.
         */
        if (buddyOffset >= poolSize ||
            blockSize > poolSize - buddyOffset) {
            break;
        }

        BuddyBlock *buddy =
            (BuddyBlock *)(poolStart + buddyOffset);

        /*
         * Buscar al compañero en la lista libre.
         */
        BuddyBlock *current = freeLists[order];
        BuddyBlock *previous = NULL;

        while (current != NULL && current != buddy) {
            previous = current;
            current = current->next;
        }

        /*
         * Si no esta libre, no se puede fusionar.
         */
        if (current == NULL) {
            break;
        }

        /*
         * Retirar al compañero de la lista libre.
         */
        if (previous == NULL) {
            freeLists[order] = current->next;
        } else {
            previous->next = current->next;
        }

        /*
         * El bloque fusionado comienza en la
         * direccion menor y tiene el orden siguiente.
         */
        if (buddyOffset < offset) {
            offset = buddyOffset;
        }

        order++;
        blockSize <<= 1;
    }

    /*
     * Incorporar el bloque resultante a su lista.
     */
    block = (BuddyBlock *)(poolStart + offset);
    block->order = order;
    block->next = freeLists[order];
    freeLists[order] = block;
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

    /*
     * Recorrer todas las listas de bloques libres.
     */
    for (uint32_t order = BUDDY_MIN_ORDER;
         order <= BUDDY_MAX_ORDER;
         order++) {

        BuddyBlock *current = freeLists[order];

        while (current != NULL) {
            freeBytes += (1ULL << order);
            current = current->next;
        }
    }

    info->totalMemory = poolSize;
    info->freeMemory = freeBytes;
    info->usedMemory = poolSize - freeBytes;
}
