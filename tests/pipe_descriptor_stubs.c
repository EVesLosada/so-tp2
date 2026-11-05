#include <stdint.h>
#include <ioDescriptor.h>

static IODescriptor testDescriptors[MAX_PROCESS_DESCRIPTORS];

IODescriptor *ioGetCurrentDescriptorTable(void) {
    return testDescriptors;
}

uint64_t ioAllocateDescriptor(
    IODescriptor *table,
    IODescriptor descriptor
) {
    if (table == 0 || !descriptor.inUse) {
        return MAX_PROCESS_DESCRIPTORS;
    }

    for (uint64_t fd = 2; fd < MAX_PROCESS_DESCRIPTORS; fd++) {
        if (!table[fd].inUse) {
            table[fd] = descriptor;
            return fd;
        }
    }

    return MAX_PROCESS_DESCRIPTORS;
}
