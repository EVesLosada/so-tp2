#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <ioDescriptor.h>
#include <pipe.h>
#include <pipeClose.h>
#include <processManager.h>

/*
 * Este test utiliza las implementaciones reales de:
 * ioDescriptor.c, pipe.c, pipeIO.c,
 * pipeSync.c y pipeClose.c.
 *
 * Solo simulamos dispositivos y scheduler.
 */

static Process reader;
static Process writer;
static Process *activeProcess = NULL;

static uint64_t pipeId;
static uint64_t readFd;
static uint64_t writeFd;
static uint64_t blockCalls;

static uint8_t consumedByte;

typedef enum {
    NO_ACTION,
    WRITE_WHILE_READER_BLOCKED,
    READ_WHILE_WRITER_BLOCKED
} TestAction;

static TestAction action = NO_ACTION;

Process *getCurrentProcess(void) {
    return activeProcess;
}

/* Mocks de dispositivos: no se utilizan en estos tests. */
uint8_t keyboardReadChar(char *character) {
    (void)character;
    return 0;
}

void terminalPutChar(char character) {
    (void)character;
}

static void activate(Process *process) {
    if (activeProcess != NULL &&
        activeProcess->state == PROCESS_RUNNING) {
        activeProcess->state = PROCESS_READY;
    }

    activeProcess = process;
    process->state = PROCESS_RUNNING;
}

/*
 * Simula el desbloqueo autorizado del administrador
 * de procesos, verificando pipe y modo.
 */
uint8_t unblockProcessFromPipe(
    uint64_t pid,
    uint64_t id,
    PipeMode mode
) {
    Process *target = NULL;

    if (pid == reader.pid) {
        target = &reader;
    } else if (pid == writer.pid) {
        target = &writer;
    }

    if (target == NULL ||
        id != pipeId ||
        target->state != PROCESS_BLOCKED ||
        target->waitingPipeId != id ||
        target->waitingPipeMode != mode) {
        return 0;
    }

    target->waitingPipeId = 0;
    target->waitingPipeMode = PIPE_READ;
    target->state = PROCESS_READY;
    return 1;
}

/*
 * Simula el cambio de contexto:
 * mientras un proceso espera, el otro utiliza
 * ioRead/ioWrite REALES sobre sus propios FDs.
 */
uint8_t blockProcess(uint64_t pid) {
    Process *blocked = activeProcess;

    assert(blocked != NULL);
    assert(blocked->pid == pid);
    assert(blocked->state == PROCESS_RUNNING);
    assert(blocked->waitingPipeId == pipeId);

    blocked->state = PROCESS_BLOCKED;
    blockCalls++;

    if (action == WRITE_WHILE_READER_BLOCKED) {
        assert(blocked == &reader);
        assert(blocked->waitingPipeMode == PIPE_READ);

        activate(&writer);

        assert(ioWrite(writeFd, "Z", 1) == 1);
    } else if (action == READ_WHILE_WRITER_BLOCKED) {
        assert(blocked == &writer);
        assert(blocked->waitingPipeMode == PIPE_WRITE);

        activate(&reader);

        char value = 0;
        assert(ioRead(readFd, &value, 1) == 1);
        consumedByte = (uint8_t)value;
    } else {
        assert(0 && "Bloqueo inesperado");
    }

    assert(blocked->state == PROCESS_READY);
    assert(blocked->waitingPipeId == 0);

    /* El scheduler vuelve a seleccionar al bloqueado. */
    activate(blocked);
    action = NO_ACTION;

    return 1;
}

static Pipe *setup(const char *name) {
    activeProcess = NULL;
    reader = (Process){0};
    writer = (Process){0};

    reader.pid = 101;
    writer.pid = 202;

    reader.state = PROCESS_READY;
    writer.state = PROCESS_READY;

    blockCalls = 0;
    consumedByte = 0;
    action = NO_ACTION;

    initializePipeTable();

    pipeId = pipeCreate(name);
    assert(pipeId != 0);

    activate(&reader);
    readFd = pipeOpenDescriptor(name, PIPE_READ);
    assert(readFd < MAX_PROCESS_DESCRIPTORS);

    activate(&writer);
    writeFd = pipeOpenDescriptor(name, PIPE_WRITE);
    assert(writeFd < MAX_PROCESS_DESCRIPTORS);

    Pipe *pipe = findPipeById(pipeId);

    assert(pipe != NULL);
    assert(pipe->readers == 1);
    assert(pipe->writers == 1);

    return pipe;
}

static void testDataAndEof(void) {
    Pipe *pipe = setup("integration-data");

    assert(ioWrite(writeFd, "HOLA", 4) == 4);
    assert(pipe->count == 4);

    /* El escritor no puede leer de su extremo. */
    char buffer[8] = {0};
    assert(ioRead(writeFd, buffer, 1) == 0);

    activate(&reader);

    /* El lector no puede escribir en su extremo. */
    assert(ioWrite(readFd, "X", 1) == 0);

    activate(&writer);

    /* Cerrar el escritor no descarta datos pendientes. */
    assert(pipeCloseCurrentDescriptor(writeFd) == 1);
    assert(pipe->writers == 0);
    assert(pipeCloseCurrentDescriptor(writeFd) == 0);

    activate(&reader);

    assert(ioRead(readFd, buffer, 4) == 4);
    assert(memcmp(buffer, "HOLA", 4) == 0);

    /* Se agotaron los datos y no quedan escritores. */
    assert(ioRead(readFd, buffer, 1) == 0);
    assert(blockCalls == 0);

    assert(pipeCloseCurrentDescriptor(readFd) == 1);
    assert(pipe->readers == 0);

    puts("OK: descriptores reales, datos pendientes y EOF");
}

static void testBlockingRead(void) {
    Pipe *pipe = setup("integration-read");

    activate(&reader);
    action = WRITE_WHILE_READER_BLOCKED;

    char output = 0;

    /*
     * El lector se bloquea.
     * El writer escribe mediante ioWrite().
     * El lector despierta y completa ioRead().
     */
    assert(ioRead(readFd, &output, 1) == 1);
    assert(output == 'Z');

    assert(blockCalls == 1);
    assert(pipe->readerWaiters.count == 0);
    assert(reader.waitingPipeId == 0);

    activate(&writer);
    assert(pipeCloseCurrentDescriptor(writeFd) == 1);

    activate(&reader);
    assert(ioRead(readFd, &output, 1) == 0);
    assert(pipeCloseCurrentDescriptor(readFd) == 1);

    puts("OK: ioRead bloqueante con ioWrite de otro proceso");
}

static void testBlockingWrite(void) {
    Pipe *pipe = setup("integration-write");

    uint8_t input[PIPE_BUFFER_SIZE + 1];
    char output[PIPE_BUFFER_SIZE];

    for (uint64_t i = 0; i < sizeof(input); i++) {
        input[i] = (uint8_t)(i % 251);
    }

    activate(&writer);
    action = READ_WHILE_WRITER_BLOCKED;

    /*
     * El escritor llena el buffer.
     * El lector consume un byte mediante ioRead().
     * El escritor despierta y escribe el ultimo.
     */
    assert(ioWrite(
        writeFd,
        (const char *)input,
        sizeof(input)
    ) == sizeof(input));

    assert(blockCalls == 1);
    assert(consumedByte == input[0]);
    assert(pipe->writerWaiters.count == 0);
    assert(writer.waitingPipeId == 0);
    assert(pipe->count == PIPE_BUFFER_SIZE);

    activate(&reader);

    assert(ioRead(
        readFd,
        output,
        sizeof(output)
    ) == PIPE_BUFFER_SIZE);

    assert(memcmp(
        output,
        input + 1,
        PIPE_BUFFER_SIZE
    ) == 0);

    activate(&writer);
    assert(pipeCloseCurrentDescriptor(writeFd) == 1);

    activate(&reader);
    assert(ioRead(readFd, output, 1) == 0);
    assert(pipeCloseCurrentDescriptor(readFd) == 1);

    puts("OK: ioWrite bloqueante con ioRead de otro proceso");
}

int main(void) {
    testDataAndEof();
    testBlockingRead();
    testBlockingWrite();

    puts("OK: test_pipe_integration");
    return 0;
}
