#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <pipeIO.h>
#include <pipeSync.h>
#include <processManager.h>

static Pipe *testPipe;
static Process reader;
static uint64_t blockCalls = 0;

Process *getCurrentProcess(void) {
    return &reader;
}

/*
 * Simula que el scheduler bloquea al lector
 * y que otro proceso escribe antes de despertarlo.
 */
uint8_t blockProcess(uint64_t pid) {
    assert(pid == reader.pid);
    assert(reader.state == PROCESS_RUNNING);
    assert(reader.waitingPipeId == testPipe->id);
    assert(reader.waitingPipeMode == PIPE_READ);
    assert(testPipe->readerWaiters.count == 1);

    reader.state = PROCESS_BLOCKED;
    blockCalls++;

    uint8_t value = 'Z';
    assert(pipeBufferWrite(testPipe, &value, 1) == 1);

    assert(pipeWakeOne(testPipe, PIPE_READ) == 1);
    assert(reader.state == PROCESS_READY);

    /* Simulamos la reanudacion del proceso. */
    reader.state = PROCESS_RUNNING;
    return 1;
}

uint8_t unblockProcessFromPipe(
    uint64_t pid,
    uint64_t pipeId,
    PipeMode mode
) {
    if (pid != reader.pid ||
        pipeId != testPipe->id ||
        mode != PIPE_READ ||
        reader.state != PROCESS_BLOCKED ||
        reader.waitingPipeId != pipeId ||
        reader.waitingPipeMode != mode) {
        return 0;
    }

    reader.waitingPipeId = 0;
    reader.state = PROCESS_READY;
    return 1;
}

int main(void) {
    initializePipeTable();

    uint64_t id = pipeCreate("read-test");
    assert(id != 0);

    testPipe = findPipeById(id);
    assert(testPipe != NULL);

    reader = (Process){0};
    reader.pid = 101;
    reader.state = PROCESS_RUNNING;

    uint8_t output = 0;

    /* Sin escritores ni datos: EOF. */
    assert(pipeReadBlocking(id, &output, 1) == 0);
    assert(blockCalls == 0);

    /* Abrir un extremo de escritura. */
    uint64_t writeFd =
        pipeOpenDescriptor("read-test", PIPE_WRITE);

    assert(writeFd != MAX_PROCESS_DESCRIPTORS);
    assert(testPipe->writers == 1);

    /*
     * Buffer vacio con escritor activo:
     * el lector debe bloquearse y despertar.
     */
    assert(pipeReadBlocking(id, &output, 1) == 1);
    assert(output == 'Z');
    assert(blockCalls == 1);
    assert(reader.waitingPipeId == 0);
    assert(testPipe->readerWaiters.count == 0);

    puts("OK: lectura bloqueante y despertar");

    /*
     * Comprobar que los datos pendientes se leen
     * incluso despues de cerrar el ultimo escritor.
     */
    uint8_t pending = 'A';
    assert(pipeBufferWrite(testPipe, &pending, 1) == 1);

    assert(pipeReleaseEndpoint(id, PIPE_WRITE) == 1);
    assert(testPipe->writers == 0);

    assert(pipeReadBlocking(id, &output, 1) == 1);
    assert(output == 'A');

    /* Ahora si: buffer vacio y EOF. */
    assert(pipeReadBlocking(id, &output, 1) == 0);
    assert(blockCalls == 1);

    puts("OK: datos pendientes antes de EOF");

    assert(pipeReadBlocking(0, &output, 1) == 0);
    assert(pipeReadBlocking(id, NULL, 1) == 0);
    assert(pipeReadBlocking(id, &output, 0) == 0);

    puts("OK: argumentos invalidos");
    puts("OK: test_pipe_read");

    return 0;
}
