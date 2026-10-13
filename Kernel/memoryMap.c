#include <stdint.h>
#include <memoryMap.h>

const E820Entry *getE820Entry(uint64_t index)
{
    const uint8_t *memoryMap =
        (const uint8_t *)E820_MAP_ADDRESS;

    return (const E820Entry *)
        (memoryMap + index * E820_ENTRY_STRIDE);
}

uint8_t isE820End(const E820Entry *entry)
{
    if (entry == 0)
        return 1;

    return entry->baseAddress == 0 &&
           entry->length == 0 &&
           entry->type == 0;
}