#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <pipe.h>
#include <ioDescriptor.h>

/*
 * Tabla simulada del proceso actual.
 * Reemplaza la dependencia de pipe.c respecto de ioDescriptor.c.
 */
static IODescriptor testDescriptors[MAX_PROCESS_DESCRIPTORS];

IODescriptor *ioGetCurrentDescriptorTable(void) {
    return testDescriptors;
}

uint64_t ioAllocateDescriptor(
    IODescriptor *table,
    IODescriptor descriptor
) {
    if (table == NULL || !descriptor.inUse) {
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

static void resetDescriptors(void) {
    for (uint64_t i = 0; i < MAX_PROCESS_DESCRIPTORS; i++) {
        testDescriptors[i] = (IODescriptor){0};
    }
}

static void testOpenReadAndWrite(void) {
    initializePipeTable();
    resetDescriptors();

    uint64_t id = pipeCreate("canal");
    assert(id != 0);

    uint64_t readFd = pipeOpenDescriptor("canal", PIPE_READ);
    uint64_t writeFd = pipeOpenDescriptor("canal", PIPE_WRITE);

    assert(readFd == 2);
    assert(writeFd == 3);

    assert(testDescriptors[readFd].type == IO_DESCRIPTOR_PIPE);
    assert(testDescriptors[readFd].permissions == IO_ACCESS_READ);
    assert(testDescriptors[readFd].resourceId == id);

    assert(testDescriptors[writeFd].type == IO_DESCRIPTOR_PIPE);
    assert(testDescriptors[writeFd].permissions == IO_ACCESS_WRITE);
    assert(testDescriptors[writeFd].resourceId == id);

    Pipe *pipe = findPipeById(id);

    assert(pipe != NULL);
    assert(pipe->readers == 1);
    assert(pipe->writers == 1);

    printf("OK: apertura de extremos de lectura y escritura\n");
}

static void testInvalidOpen(void) {
    initializePipeTable();
    resetDescriptors();

    assert(
        pipeOpenDescriptor("inexistente", PIPE_READ)
        == MAX_PROCESS_DESCRIPTORS
    );

    uint64_t id = pipeCreate("valido");
    assert(id != 0);

    assert(
        pipeOpenDescriptor("valido", (PipeMode)99)
        == MAX_PROCESS_DESCRIPTORS
    );

    Pipe *pipe = findPipeById(id);
    assert(pipe->readers == 0);
    assert(pipe->writers == 0);

    printf("OK: apertura invalida\n");
}

static void testFullDescriptorTable(void) {
    initializePipeTable();
    resetDescriptors();

    uint64_t id = pipeCreate("sin-espacio");
    assert(id != 0);

    for (uint64_t fd = 2; fd < MAX_PROCESS_DESCRIPTORS; fd++) {
        testDescriptors[fd].inUse = 1;
    }

    assert(
        pipeOpenDescriptor("sin-espacio", PIPE_READ)
        == MAX_PROCESS_DESCRIPTORS
    );

    Pipe *pipe = findPipeById(id);

    assert(pipe != NULL);
    assert(pipe->readers == 0);
    assert(pipe->writers == 0);

    printf("OK: tabla de descriptores llena\n");
}


static void testReleaseEndpoint(void) {
    initializePipeTable();
    resetDescriptors();

    uint64_t id = pipeCreate("release-test");
    assert(id != 0);

    uint64_t readFd = pipeOpenDescriptor("release-test", PIPE_READ);
    uint64_t writeFd = pipeOpenDescriptor("release-test", PIPE_WRITE);

    assert(readFd != MAX_PROCESS_DESCRIPTORS);
    assert(writeFd != MAX_PROCESS_DESCRIPTORS);

    Pipe *pipe = findPipeById(id);
    assert(pipe != NULL);

    assert(pipe->readers == 1);
    assert(pipe->writers == 1);

    assert(pipeReleaseEndpoint(id, PIPE_READ) == 1);
    assert(pipe->readers == 0);
    assert(pipe->writers == 1);

    assert(pipeReleaseEndpoint(id, PIPE_WRITE) == 1);
    assert(pipe->writers == 0);

    /* No se puede liberar una referencia inexistente. */
    assert(pipeReleaseEndpoint(id, PIPE_READ) == 0);
    assert(pipeReleaseEndpoint(id, PIPE_WRITE) == 0);

    /* Tampoco se aceptan IDs o modos invalidos. */
    assert(pipeReleaseEndpoint(0, PIPE_READ) == 0);
    assert(pipeReleaseEndpoint(id, (PipeMode)99) == 0);

    assert(pipe->readers == 0);
    assert(pipe->writers == 0);

    printf("OK: liberacion de referencias\n");
}

int main(void) {
    testOpenReadAndWrite();
    testInvalidOpen();
    testFullDescriptorTable();
    testReleaseEndpoint();

    printf("OK: test_pipe_descriptors\n");
    return 0;
}
