#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pipeIO.h>
#include <pipeSync.h>
#include <processManager.h>

static Pipe *testPipe;
static Process writer;
static uint64_t blockCalls = 0;
static uint8_t closeReaderOnBlock = 0;
static uint8_t firstBytes[2];

Process *getCurrentProcess(void) {
    return &writer;
}

uint8_t unblockProcessFromPipe(
    uint64_t pid,
    uint64_t pipeId,
    PipeMode mode
) {
    if (pid != writer.pid ||
        pipeId != testPipe->id ||
        mode != PIPE_WRITE ||
        writer.state != PROCESS_BLOCKED ||
        writer.waitingPipeId != pipeId ||
        writer.waitingPipeMode != mode) {
        return 0;
    }

    writer.waitingPipeId = 0;
    writer.state = PROCESS_READY;
    return 1;
}

/*
 * Simula el cambio de contexto:
 * - un lector consume dos bytes, o
 * - el ultimo lector cierra su extremo.
 */
uint8_t blockProcess(uint64_t pid) {
    assert(pid == writer.pid);
    assert(writer.state == PROCESS_RUNNING);
    assert(writer.waitingPipeId == testPipe->id);
    assert(writer.waitingPipeMode == PIPE_WRITE);
    assert(testPipe->writerWaiters.count == 1);

    writer.state = PROCESS_BLOCKED;
    blockCalls++;

    if (closeReaderOnBlock) {
        assert(pipeReleaseEndpoint(
            testPipe->id, PIPE_READ
        ) == 1);

        assert(pipeWakeAll(testPipe, PIPE_WRITE) == 1);
    } else {
        assert(pipeBufferRead(
            testPipe, firstBytes, 2
        ) == 2);

        assert(pipeWakeOne(testPipe, PIPE_WRITE) == 1);
    }

    assert(writer.state == PROCESS_READY);

    writer.state = PROCESS_RUNNING;
    return 1;
}

static void preparePipe(const char *name) {
    initializePipeTable();

    uint64_t id = pipeCreate(name);
    assert(id != 0);

    testPipe = findPipeById(id);
    assert(testPipe != NULL);

    writer = (Process){0};
    writer.pid = 101;
    writer.state = PROCESS_RUNNING;

    blockCalls = 0;
    closeReaderOnBlock = 0;
}

static void testWriteAndWakeup(void) {
    preparePipe("write-test");

    uint8_t input[PIPE_BUFFER_SIZE + 2];
    uint8_t output[PIPE_BUFFER_SIZE];

    for (uint64_t i = 0; i < sizeof(input); i++) {
        input[i] = (uint8_t)(i % 251);
    }

    /* Sin lectores, no se escribe. */
    assert(pipeWriteBlocking(
        testPipe->id, input, sizeof(input)
    ) == 0);

    uint64_t readFd = pipeOpenDescriptor(
        "write-test", PIPE_READ
    );
    assert(readFd != MAX_PROCESS_DESCRIPTORS);

    uint64_t writeFd = pipeOpenDescriptor(
        "write-test", PIPE_WRITE
    );
    assert(writeFd != MAX_PROCESS_DESCRIPTORS);

    /*
     * Se escriben 512 bytes, el escritor espera,
     * un lector libera dos lugares y se completa
     * la escritura de los 514 bytes.
     */
    assert(pipeWriteBlocking(
        testPipe->id, input, sizeof(input)
    ) == sizeof(input));

    assert(blockCalls == 1);
    assert(testPipe->writerWaiters.count == 0);
    assert(writer.waitingPipeId == 0);

    assert(firstBytes[0] == input[0]);
    assert(firstBytes[1] == input[1]);

    assert(testPipe->count == PIPE_BUFFER_SIZE);

    assert(pipeBufferRead(
        testPipe, output, sizeof(output)
    ) == PIPE_BUFFER_SIZE);

    assert(memcmp(
        output,
        input + 2,
        PIPE_BUFFER_SIZE
    ) == 0);

    puts("OK: escritura bloqueante y despertar");
}

static void testReaderClosesWhileBlocked(void) {
    preparePipe("close-test");

    uint64_t readFd = pipeOpenDescriptor(
        "close-test", PIPE_READ
    );
    assert(readFd != MAX_PROCESS_DESCRIPTORS);

    uint64_t writeFd = pipeOpenDescriptor(
        "close-test", PIPE_WRITE
    );
    assert(writeFd != MAX_PROCESS_DESCRIPTORS);

    uint8_t input[PIPE_BUFFER_SIZE + 1];

    for (uint64_t i = 0; i < sizeof(input); i++) {
        input[i] = (uint8_t)(i % 251);
    }

    closeReaderOnBlock = 1;

    /*
     * El escritor llena el buffer, se bloquea
     * y el ultimo lector cierra.
     *
     * Debe devolver los 512 bytes ya escritos
     * sin intentar escribir el restante.
     */
    assert(pipeWriteBlocking(
        testPipe->id, input, sizeof(input)
    ) == PIPE_BUFFER_SIZE);

    assert(blockCalls == 1);
    assert(testPipe->readers == 0);
    assert(testPipe->writerWaiters.count == 0);

    puts("OK: cierre del ultimo lector durante escritura");
}

int main(void) {
    testWriteAndWakeup();
    testReaderClosesWhileBlocked();

    puts("OK: test_pipe_write");
    return 0;
}
