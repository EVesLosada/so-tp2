#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <mvar.h>

typedef struct {
    uint8_t inUse;
    uint64_t value;
} MockSemaphore;

static MockSemaphore semaphores[4];

uint64_t sysSemCreate(const char *name, uint64_t initialValue) {
    if (strcmp(name, "fail") == 0) {
        return 0;
    }

    for (uint64_t i = 0; i < 4; i++) {
        if (!semaphores[i].inUse) {
            semaphores[i].inUse = 1;
            semaphores[i].value = initialValue;
            return i + 1;
        }
    }

    return 0;
}

uint64_t sysSemWait(uint64_t id) {
    if (id == 0 || id > 4 ||
        !semaphores[id - 1].inUse ||
        semaphores[id - 1].value == 0) {
        return 0;
    }

    semaphores[id - 1].value--;
    return 1;
}

uint64_t sysSemPost(uint64_t id) {
    if (id == 0 || id > 4 ||
        !semaphores[id - 1].inUse) {
        return 0;
    }

    semaphores[id - 1].value++;
    return 1;
}

uint64_t sysSemClose(uint64_t id) {
    if (id == 0 || id > 4 ||
        !semaphores[id - 1].inUse) {
        return 0;
    }

    semaphores[id - 1].inUse = 0;
    semaphores[id - 1].value = 0;
    return 1;
}

static uint64_t activeSemaphores(void) {
    uint64_t count = 0;

    for (uint64_t i = 0; i < 4; i++) {
        count += semaphores[i].inUse;
    }

    return count;
}

int main(void) {
    MVar mvar = {0};
    uint64_t received = 0;

    assert(!mvarPut(&mvar, 1));
    assert(!mvarTake(&mvar, &received));
    assert(!mvarClose(&mvar));

    assert(!mvarInit(0, "empty", "full"));
    assert(!mvarInit(&mvar, 0, "full"));

    assert(mvarInit(&mvar, "empty", "full"));
    assert(activeSemaphores() == 2);

    /*
     * El mock no bloquea: devuelve 0 cuando
     * el semaforo no tiene recursos.
     */
    assert(!mvarTake(&mvar, &received));

    for (uint64_t i = 0; i < 100; i++) {
        assert(mvarPut(&mvar, i));
        assert(mvarTake(&mvar, &received));
        assert(received == i);
    }

    assert(mvarClose(&mvar));
    assert(activeSemaphores() == 0);
    assert(!mvarClose(&mvar));

    assert(!mvarInit(&mvar, "empty", "fail"));
    assert(activeSemaphores() == 0);

    puts("OK: test_mvar");
    return 0;
}
