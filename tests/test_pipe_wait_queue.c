#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <pipe.h>

int main(void) {
    initializePipeTable();

    uint64_t id = pipeCreate("queue-test");
    assert(id != 0);

    Pipe *pipe = findPipeById(id);
    assert(pipe != NULL);

    /* Las colas empiezan vacias. */
    assert(pipe->readerWaiters.count == 0);
    assert(pipe->writerWaiters.count == 0);

    /* Lectores y escritores tienen colas independientes. */
    assert(pipeEnqueueWaiter(pipe, PIPE_READ, 101) == 1);
    assert(pipeEnqueueWaiter(pipe, PIPE_READ, 102) == 1);
    assert(pipeEnqueueWaiter(pipe, PIPE_READ, 103) == 1);
    assert(pipeEnqueueWaiter(pipe, PIPE_WRITE, 201) == 1);

    assert(pipe->readerWaiters.count == 3);
    assert(pipe->writerWaiters.count == 1);

    /* No se admiten duplicados ni PIDs invalidos. */
    assert(pipeEnqueueWaiter(pipe, PIPE_READ, 101) == 0);
    assert(pipeEnqueueWaiter(pipe, PIPE_READ, 0) == 0);
    assert(pipeEnqueueWaiter(pipe, (PipeMode)99, 202) == 0);
    assert(pipeEnqueueWaiter(NULL, PIPE_READ, 202) == 0);

    /* Desencolar respeta el orden FIFO. */
    assert(pipeDequeueWaiter(pipe, PIPE_READ) == 101);

    /* Eliminar uno del medio conserva el orden. */
    assert(pipeRemoveWaiter(pipe, PIPE_READ, 102) == 1);
    assert(pipeRemoveWaiter(pipe, PIPE_READ, 102) == 0);

    assert(pipeDequeueWaiter(pipe, PIPE_READ) == 103);
    assert(pipeDequeueWaiter(pipe, PIPE_READ) == 0);

    /* La cola de escritores no se vio afectada. */
    assert(pipeDequeueWaiter(pipe, PIPE_WRITE) == 201);
    assert(pipeDequeueWaiter(pipe, PIPE_WRITE) == 0);

    puts("OK: colas independientes, FIFO y cancelacion");

    /* Comprobar capacidad maxima. */
    for (uint64_t i = 0; i < PIPE_MAX_WAITERS; i++) {
        assert(pipeEnqueueWaiter(pipe, PIPE_READ, 1000 + i) == 1);
    }

    assert(pipe->readerWaiters.count == PIPE_MAX_WAITERS);
    assert(pipeEnqueueWaiter(pipe, PIPE_READ, 9999) == 0);

    /* Liberar un lugar y volver a encolar. */
    uint64_t middle = PIPE_MAX_WAITERS / 2;

    assert(pipeRemoveWaiter(pipe, PIPE_READ, 1000 + middle) == 1);
    assert(pipeEnqueueWaiter(pipe, PIPE_READ, 9999) == 1);

    for (uint64_t i = 0; i < PIPE_MAX_WAITERS; i++) {
        if (i == middle) {
            continue;
        }
        assert(pipeDequeueWaiter(pipe, PIPE_READ) == 1000 + i);
    }

    assert(pipeDequeueWaiter(pipe, PIPE_READ) == 9999);
    assert(pipeDequeueWaiter(pipe, PIPE_READ) == 0);
    assert(pipe->readerWaiters.count == 0);

    puts("OK: capacidad maxima y reutilizacion de espacio");
    puts("OK: test_pipe_wait_queue");

    return 0;
}
