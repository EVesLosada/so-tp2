#include <pipeIO.h>
#include <stdint.h>
#include <stddef.h>

#include <ioDescriptor.h>
#include <processManager.h>
#include <keyboard.h>
#include <terminal.h>

/*
 * La shell inicial no posee PCB.
 * Por eso mantiene su propia tabla de descriptores.
 */
static IODescriptor shellDescriptors[MAX_PROCESS_DESCRIPTORS] = {
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

    if (current == NULL) {
        return shellDescriptors;
    }

    return current->ioDescriptors;
}

/*
 * Comprueba que la descripcion del recurso sea coherente.
 */
static uint8_t isValidDescriptor(const IODescriptor *descriptor) {
    if (descriptor == NULL || !descriptor->inUse) {
        return 0;
    }

    if (descriptor->permissions == 0 ||
        (descriptor->permissions &
         ~(IO_ACCESS_READ | IO_ACCESS_WRITE)) != 0) {
        return 0;
    }

    switch (descriptor->type) {
        case IO_DESCRIPTOR_KEYBOARD:
            return descriptor->permissions == IO_ACCESS_READ &&
                   descriptor->resourceId == 0;

        case IO_DESCRIPTOR_TERMINAL:
            return descriptor->permissions == IO_ACCESS_WRITE &&
                   descriptor->resourceId == 0;

        case IO_DESCRIPTOR_PIPE:
            /*
             * Un extremo de pipe es de lectura O escritura.
             * La existencia del pipe se comprobara cuando
             * se implemente el subsistema correspondiente.
             */
            return descriptor->resourceId != 0 &&
                   (descriptor->permissions == IO_ACCESS_READ ||
                    descriptor->permissions == IO_ACCESS_WRITE);

        default:
            return 0;
    }
}

uint8_t ioGetDescriptor(
    const IODescriptor *table,
    uint64_t fd,
    IODescriptor *result
) {
    if (table == NULL ||
        result == NULL ||
        fd >= MAX_PROCESS_DESCRIPTORS ||
        !isValidDescriptor(&table[fd])) {
        return 0;
    }

    *result = table[fd];
    return 1;
}

uint8_t ioSetDescriptor(
    IODescriptor *table,
    uint64_t fd,
    IODescriptor descriptor
) {
    if (table == NULL ||
        fd >= MAX_PROCESS_DESCRIPTORS ||
        !isValidDescriptor(&descriptor)) {
        return 0;
    }

    /*
     * Esta operacion modifica el mapeo local.
     * La gestion de referencias de pipes se integrara
     * en los bloques siguientes.
     */
    table[fd] = descriptor;
    return 1;
}


uint64_t ioAllocateDescriptor(
    IODescriptor *table,
    IODescriptor descriptor
) {
    if (table == NULL || !isValidDescriptor(&descriptor)) {
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

uint8_t ioUnmapDescriptor(IODescriptor *table, uint64_t fd) {
    if (table == NULL ||
        fd >= MAX_PROCESS_DESCRIPTORS ||
        !table[fd].inUse) {
        return 0;
    }

    table[fd] = (IODescriptor){0};
    return 1;
}

uint64_t ioRead(uint64_t fd, char *buffer, uint64_t count) {
    if (buffer == NULL || count == 0) {
        return 0;
    }

    IODescriptor descriptor;

    if (!ioGetDescriptor(
            ioGetCurrentDescriptorTable(),
            fd,
            &descriptor) ||
        !(descriptor.permissions & IO_ACCESS_READ)) {
        return 0;
    }

    switch (descriptor.type) {
        case IO_DESCRIPTOR_KEYBOARD: {
            uint64_t readCount = 0;

            while (readCount < count &&
                   keyboardReadChar(&buffer[readCount])) {
                readCount++;
            }

            return readCount;
        }

        case IO_DESCRIPTOR_PIPE:
            return pipeReadBlocking(
                descriptor.resourceId,
                (uint8_t *)buffer,
                count
            );

        default:
            return 0;
    }
}

uint64_t ioWrite(uint64_t fd, const char *buffer, uint64_t count) {
    if (buffer == NULL) {
        return 0;
    }

    IODescriptor descriptor;

    if (!ioGetDescriptor(
            ioGetCurrentDescriptorTable(),
            fd,
            &descriptor) ||
        !(descriptor.permissions & IO_ACCESS_WRITE)) {
        return 0;
    }

    switch (descriptor.type) {
        case IO_DESCRIPTOR_TERMINAL:
            for (uint64_t i = 0; i < count; i++) {
                terminalPutChar(buffer[i]);
            }

            return count;

        case IO_DESCRIPTOR_PIPE:
            return pipeWriteBlocking(
                descriptor.resourceId,
                (const uint8_t *)buffer,
                count
            );

        default:
            return 0;
    }
}
