#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <pipe.h>

static void testCreation(void) {
    initializePipeTable();

    uint64_t id = pipeCreate("test-pipe");

    assert(id != 0);
    assert(pipeOpen("test-pipe") == id);
    assert(findPipeById(id) != NULL);

    assert(pipeCreate("test-pipe") == 0);
    assert(pipeOpen("inexistente") == 0);
    assert(pipeCreate(NULL) == 0);
    assert(pipeCreate("") == 0);

    printf("OK: creacion y apertura de pipes\n");
}

static void testBuffer(void) {
    initializePipeTable();

    uint64_t id = pipeCreate("buffer-test");
    Pipe *pipe = findPipeById(id);

    assert(pipe != NULL);

    uint8_t input[] = {'H', 'O', 'L', 'A'};
    uint8_t output[4] = {0};

    assert(pipeBufferWrite(pipe, input, 4) == 4);
    assert(pipe->count == 4);

    assert(pipeBufferRead(pipe, output, 4) == 4);
    assert(pipe->count == 0);

    for (uint64_t i = 0; i < 4; i++) {
        assert(output[i] == input[i]);
    }

    printf("OK: escritura y lectura FIFO\n");
}

static void testCircularBuffer(void) {
    initializePipeTable();

    uint64_t id = pipeCreate("circular-test");
    Pipe *pipe = findPipeById(id);

    assert(pipe != NULL);

    uint8_t input[PIPE_BUFFER_SIZE];
    uint8_t output[PIPE_BUFFER_SIZE];

    for (uint64_t i = 0; i < PIPE_BUFFER_SIZE; i++) {
        input[i] = (uint8_t)(i % 256);
    }

    assert(
        pipeBufferWrite(pipe, input, PIPE_BUFFER_SIZE)
        == PIPE_BUFFER_SIZE
    );

    assert(pipe->count == PIPE_BUFFER_SIZE);

    uint8_t extra = 99;
    assert(pipeBufferWrite(pipe, &extra, 1) == 0);

    assert(
        pipeBufferRead(pipe, output, PIPE_BUFFER_SIZE)
        == PIPE_BUFFER_SIZE
    );

    for (uint64_t i = 0; i < PIPE_BUFFER_SIZE; i++) {
        assert(output[i] == input[i]);
    }

    assert(pipe->count == 0);

    /* Comprobar reutilizacion del buffer. */
    assert(pipeBufferWrite(pipe, input, 4) == 4);
    assert(pipeBufferRead(pipe, output, 4) == 4);

    for (uint64_t i = 0; i < 4; i++) {
        assert(output[i] == input[i]);
    }

    printf("OK: buffer circular y capacidad maxima\n");
}

static void testCapacity(void) {
    initializePipeTable();

    for (uint64_t i = 0; i < MAX_PIPES; i++) {
        char name[PIPE_NAME_MAX_LENGTH];

        snprintf(
            name,
            sizeof(name),
            "pipe-%llu",
            (unsigned long long)i
        );

        assert(pipeCreate(name) != 0);
    }

    assert(pipeCreate("extra-pipe") == 0);

    printf("OK: capacidad maxima de pipes\n");
}


static void testCircularWraparound(void) {
    initializePipeTable();

    uint64_t id = pipeCreate("wraparound-test");
    assert(id != 0);

    Pipe *pipe = findPipeById(id);
    assert(pipe != NULL);

    uint8_t first[500];
    uint8_t second[300];
    uint8_t output[400];

    for (uint64_t i = 0; i < 500; i++) {
        first[i] = (uint8_t)(i % 251);
    }

    for (uint64_t i = 0; i < 300; i++) {
        second[i] = (uint8_t)(100 + i % 151);
    }

    assert(pipeBufferWrite(pipe, first, 500) == 500);
    assert(pipeBufferRead(pipe, output, 400) == 400);

    for (uint64_t i = 0; i < 400; i++) {
        assert(output[i] == first[i]);
    }

    assert(pipe->count == 100);

    /* El indice de escritura debe volver al inicio. */
    assert(pipeBufferWrite(pipe, second, 300) == 300);
    assert(pipe->count == 400);

    assert(pipeBufferRead(pipe, output, 400) == 400);

    /* Primero deben salir los 100 bytes antiguos. */
    for (uint64_t i = 0; i < 100; i++) {
        assert(output[i] == first[400 + i]);
    }

    /* Despues deben salir los 300 bytes nuevos. */
    for (uint64_t i = 0; i < 300; i++) {
        assert(output[100 + i] == second[i]);
    }

    assert(pipe->count == 0);

    printf("OK: buffer circular con datos pendientes\n");
}

int main(void) {
    testCircularWraparound();
    testCreation();
    testBuffer();
    testCircularBuffer();
    testCapacity();

    printf("OK: test_pipe\n");
    return 0;
}
