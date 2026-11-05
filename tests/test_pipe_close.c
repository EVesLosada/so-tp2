#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include <pipe.h>
#include <pipeClose.h>

/*
 * Tabla de descriptores simulada.
 * Probamos la logica real de pipeClose.c.
 */
static IODescriptor descriptors[MAX_PROCESS_DESCRIPTORS];

IODescriptor *ioGetCurrentDescriptorTable(void) {
    return descriptors;
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

uint8_t ioGetDescriptor(
    const IODescriptor *table,
    uint64_t fd,
    IODescriptor *result
) {
    if (table == NULL ||
        result == NULL ||
        fd >= MAX_PROCESS_DESCRIPTORS ||
        !table[fd].inUse) {
        return 0;
    }

    *result = table[fd];
    return 1;
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

/* Simulacion del despertar autorizado. */
static uint64_t expectedPipeId;
static uint64_t readerWakeups;
static uint64_t writerWakeups;

uint8_t unblockProcessFromPipe(
    uint64_t pid,
    uint64_t pipeId,
    PipeMode mode
) {
    if (pipeId != expectedPipeId) {
        return 0;
    }

    if (mode == PIPE_READ && pid == 101) {
        readerWakeups++;
        return 1;
    }

    if (mode == PIPE_WRITE && pid == 201) {
        writerWakeups++;
        return 1;
    }

    /* Cualquier otro PID representa una espera obsoleta. */
    return 0;
}

static void resetTest(void) {
    for (uint64_t i = 0; i < MAX_PROCESS_DESCRIPTORS; i++) {
        descriptors[i] = (IODescriptor){0};
    }

    initializePipeTable();
    expectedPipeId = 0;
    readerWakeups = 0;
    writerWakeups = 0;
}

static void testLastWriter(void) {
    resetTest();

    uint64_t id = pipeCreate("close-writer");
    assert(id != 0);

    Pipe *pipe = findPipeById(id);
    assert(pipe != NULL);
    expectedPipeId = id;

    uint64_t readFd =
        pipeOpenDescriptor("close-writer", PIPE_READ);

    uint64_t writeFd1 =
        pipeOpenDescriptor("close-writer", PIPE_WRITE);

    uint64_t writeFd2 =
        pipeOpenDescriptor("close-writer", PIPE_WRITE);

    assert(readFd < MAX_PROCESS_DESCRIPTORS);
    assert(writeFd1 < MAX_PROCESS_DESCRIPTORS);
    assert(writeFd2 < MAX_PROCESS_DESCRIPTORS);

    assert(pipe->readers == 1);
    assert(pipe->writers == 2);

    /* Un PID obsoleto y un lector valido. */
    assert(pipeEnqueueWaiter(pipe, PIPE_READ, 999) == 1);
    assert(pipeEnqueueWaiter(pipe, PIPE_READ, 101) == 1);

    /* Cerrar un escritor no provoca EOF todavia. */
    assert(pipeCloseCurrentDescriptor(writeFd1) == 1);
    assert(pipe->writers == 1);
    assert(readerWakeups == 0);
    assert(pipe->readerWaiters.count == 2);

    /* Cerrar el ultimo escritor despierta al lector. */
    assert(pipeCloseCurrentDescriptor(writeFd2) == 1);
    assert(pipe->writers == 0);
    assert(readerWakeups == 1);
    assert(pipe->readerWaiters.count == 0);

    /* Un descriptor cerrado no se cierra dos veces. */
    assert(pipeCloseCurrentDescriptor(writeFd2) == 0);
    assert(pipe->writers == 0);

    IODescriptor descriptor;
    assert(ioGetDescriptor(descriptors, writeFd2, &descriptor) == 0);

    assert(pipeCloseCurrentDescriptor(readFd) == 1);
    assert(pipe->readers == 0);

    puts("OK: ultimo escritor despierta lectores");
    puts("OK: doble cierre rechazado");
}

static void testLastReader(void) {
    resetTest();

    uint64_t id = pipeCreate("close-reader");
    assert(id != 0);

    Pipe *pipe = findPipeById(id);
    assert(pipe != NULL);
    expectedPipeId = id;

    uint64_t readFd =
        pipeOpenDescriptor("close-reader", PIPE_READ);

    uint64_t writeFd =
        pipeOpenDescriptor("close-reader", PIPE_WRITE);

    assert(readFd < MAX_PROCESS_DESCRIPTORS);
    assert(writeFd < MAX_PROCESS_DESCRIPTORS);

    assert(pipeEnqueueWaiter(pipe, PIPE_WRITE, 888) == 1);
    assert(pipeEnqueueWaiter(pipe, PIPE_WRITE, 201) == 1);

    /* El ultimo lector desaparece. */
    assert(pipeCloseDescriptor(descriptors, readFd) == 1);

    assert(pipe->readers == 0);
    assert(pipe->writers == 1);
    assert(writerWakeups == 1);
    assert(pipe->writerWaiters.count == 0);

    assert(pipeCloseDescriptor(descriptors, readFd) == 0);
    assert(pipeCloseDescriptor(descriptors, writeFd) == 1);

    puts("OK: ultimo lector despierta escritores");
}

static void testCloseAllAndInvalidDescriptors(void) {
    resetTest();

    assert(pipeCloseDescriptor(NULL, 2) == 0);
    assert(pipeCloseCurrentDescriptor(0) == 0);
    assert(pipeCloseCurrentDescriptor(1) == 0);
    assert(pipeCloseCurrentDescriptor(MAX_PROCESS_DESCRIPTORS) == 0);

    /* No debe cerrar un descriptor que no sea de pipe. */
    descriptors[2] = (IODescriptor){
        .inUse = 1,
        .permissions = IO_ACCESS_WRITE,
        .type = IO_DESCRIPTOR_TERMINAL
    };

    assert(pipeCloseCurrentDescriptor(2) == 0);
    assert(descriptors[2].inUse == 1);
    descriptors[2] = (IODescriptor){0};

    uint64_t id = pipeCreate("close-all");
    assert(id != 0);

    Pipe *pipe = findPipeById(id);
    assert(pipe != NULL);

    uint64_t readFd =
        pipeOpenDescriptor("close-all", PIPE_READ);

    uint64_t writeFd =
        pipeOpenDescriptor("close-all", PIPE_WRITE);

    assert(readFd < MAX_PROCESS_DESCRIPTORS);
    assert(writeFd < MAX_PROCESS_DESCRIPTORS);

    assert(pipe->readers == 1);
    assert(pipe->writers == 1);

    pipeCloseAllDescriptors(descriptors);

    assert(pipe->readers == 0);
    assert(pipe->writers == 0);

    for (uint64_t fd = 2; fd < MAX_PROCESS_DESCRIPTORS; fd++) {
        assert(descriptors[fd].inUse == 0);
    }

    /* Una segunda limpieza no altera las referencias. */
    pipeCloseAllDescriptors(descriptors);
    assert(pipe->readers == 0);
    assert(pipe->writers == 0);

    puts("OK: limpieza de tabla e invalidos");
}


static void testProcessPipeCleanup(void) {
    resetTest();

    uint64_t id = pipeCreate("process-clean");
    assert(id != 0);

    Pipe *pipe = findPipeById(id);
    assert(pipe != NULL);

    expectedPipeId = id;

    uint64_t readFd =
        pipeOpenDescriptor("process-clean", PIPE_READ);

    uint64_t writeFd =
        pipeOpenDescriptor("process-clean", PIPE_WRITE);

    assert(readFd < MAX_PROCESS_DESCRIPTORS);
    assert(writeFd < MAX_PROCESS_DESCRIPTORS);
    assert(readFd != writeFd);

    /*
     * Simular que los extremos pertenecen al PCB
     * en lugar de a la tabla de prueba de la shell.
     */
    Process process = {0};
    process.pid = 707;

    process.ioDescriptors[2] = descriptors[readFd];
    process.ioDescriptors[3] = descriptors[writeFd];

    descriptors[readFd] = (IODescriptor){0};
    descriptors[writeFd] = (IODescriptor){0};

    assert(pipe->readers == 1);
    assert(pipe->writers == 1);

    process.waitingPipeId = id;
    process.waitingPipeMode = PIPE_READ;

    assert(pipeEnqueueWaiter(pipe, PIPE_READ, process.pid) == 1);
    assert(pipe->readerWaiters.count == 1);

    pipeCleanupProcess(&process);

    assert(process.waitingPipeId == 0);
    assert(pipe->readerWaiters.count == 0);
    assert(pipe->readers == 0);
    assert(pipe->writers == 0);

    assert(process.ioDescriptors[2].inUse == 0);
    assert(process.ioDescriptors[3].inUse == 0);

    /* Una segunda limpieza no debe afectar los contadores. */
    pipeCleanupProcess(&process);

    assert(pipe->readers == 0);
    assert(pipe->writers == 0);

    pipeCleanupProcess(NULL);

    puts("OK: limpieza de pipes al terminar un proceso");
}

int main(void) {
    testLastWriter();
    testLastReader();
    testCloseAllAndInvalidDescriptors();

    testProcessPipeCleanup();

    puts("OK: test_pipe_close");
    return 0;
}
