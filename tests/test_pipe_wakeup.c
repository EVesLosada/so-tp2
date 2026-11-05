#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include <pipeSync.h>
#include <processManager.h>

/*
 * Simula el desbloqueo autorizado.
 * El PID 111 representa un proceso terminado.
 */
static uint64_t expectedPipeId = 0;
static uint64_t awakenedPids[8];
static uint64_t awakenedCount = 0;

uint8_t unblockProcessFromPipe(
    uint64_t pid,
    uint64_t pipeId,
    PipeMode mode
) {
    if (pipeId != expectedPipeId ||
        pid == 111 ||
        (mode != PIPE_READ && mode != PIPE_WRITE)) {
        return 0;
    }

    assert(awakenedCount < 8);
    awakenedPids[awakenedCount++] = pid;
    return 1;
}

int main(void) {
    initializePipeTable();

    uint64_t id = pipeCreate("wakeup-test");
    assert(id != 0);

    Pipe *pipe = findPipeById(id);
    assert(pipe != NULL);

    expectedPipeId = id;

    assert(pipeWakeOne(NULL, PIPE_READ) == 0);
    assert(pipeWakeAll(pipe, (PipeMode)99) == 0);
    assert(pipeWakeOne(pipe, PIPE_READ) == 0);

    /* El primer PID es obsoleto. */
    assert(pipeEnqueueWaiter(pipe, PIPE_READ, 111) == 1);
    assert(pipeEnqueueWaiter(pipe, PIPE_READ, 222) == 1);
    assert(pipeEnqueueWaiter(pipe, PIPE_READ, 333) == 1);

    assert(pipeWakeOne(pipe, PIPE_READ) == 1);
    assert(awakenedCount == 1);
    assert(awakenedPids[0] == 222);

    assert(pipe->readerWaiters.count == 1);
    assert(pipe->readerWaiters.pids[0] == 333);

    assert(pipeWakeAll(pipe, PIPE_READ) == 1);
    assert(awakenedCount == 2);
    assert(awakenedPids[1] == 333);
    assert(pipe->readerWaiters.count == 0);

    puts("OK: despertar FIFO y descarte de PID obsoleto");

    /* Despertar a todos los escritores. */
    assert(pipeEnqueueWaiter(pipe, PIPE_WRITE, 444) == 1);
    assert(pipeEnqueueWaiter(pipe, PIPE_WRITE, 555) == 1);

    assert(pipeWakeAll(pipe, PIPE_WRITE) == 2);
    assert(awakenedCount == 4);
    assert(awakenedPids[2] == 444);
    assert(awakenedPids[3] == 555);

    assert(pipe->writerWaiters.count == 0);
    assert(pipeWakeAll(pipe, PIPE_WRITE) == 0);

    puts("OK: despertar de todos los escritores");
    puts("OK: test_pipe_wakeup");

    return 0;
}
