#include <stdint.h>
#include <process.h>
#include <processManager.h>
#include <ioDescriptor.h>
#include <pipe.h>
#include <pipeClose.h>
#include <stddef.h>

/*
 * Simulaciones para los tests que compilan processManager.c
 * sin enlazar los modulos reales de pipes y descriptores.
 */
static IODescriptor testShellDescriptors[MAX_PROCESS_DESCRIPTORS] = {
    [IO_FD_STDIN] = {
        .inUse = 1,
        .permissions = IO_ACCESS_READ,
        .type = IO_DESCRIPTOR_KEYBOARD,
        .resourceId = 0
    },
    [IO_FD_STDOUT] = {
        .inUse = 1,
        .permissions = IO_ACCESS_WRITE,
        .type = IO_DESCRIPTOR_TERMINAL,
        .resourceId = 0
    }
};

IODescriptor *ioGetCurrentDescriptorTable(void) {
    Process *current = getCurrentProcess();

    if (current != NULL) {
        return current->ioDescriptors;
    }

    return testShellDescriptors;
}

uint8_t ioGetDescriptor(
    const IODescriptor *table,
    uint64_t fd,
    IODescriptor *descriptor
) {
    if (table == NULL ||
        descriptor == NULL ||
        fd >= MAX_PROCESS_DESCRIPTORS ||
        !table[fd].inUse) {
        return 0;
    }

    *descriptor = table[fd];
    return 1;
}

Pipe *findPipeById(uint64_t id) {
    (void)id;
    return NULL;
}

void pipeCloseAllDescriptors(IODescriptor *table) {
    (void)table;
}

void pipeCleanupProcess(Process *process) {
    (void)process;
}

/* Stub para tests que no enlazan el subsistema real de semaforos. */
__attribute__((weak))
uint8_t semCancelWaiter(uint64_t pid) {
    (void)pid;
    return 0;
}
