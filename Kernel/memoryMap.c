#include <stdint.h>
#include <memoryMap.h>

const E820Entry *getE820Entry(uint64_t index)
{
    const E820Entry *memoryMap =
        (const E820Entry *)E820_MAP_ADDRESS;

    return &memoryMap[index];
}

uint8_t isE820End(const E820Entry *entry)
{
    if (entry == 0)
        return 1;

    return entry->baseAddress == 0 &&
           entry->length == 0 &&
           entry->type == 0;
}