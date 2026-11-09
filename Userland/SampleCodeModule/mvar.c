#include <mvar.h>
#include <syscalls.h>

uint8_t mvarInit(
    MVar *mvar,
    const char *emptyName,
    const char *fullName
) {
    if (mvar == 0 || emptyName == 0 || fullName == 0) {
        return 0;
    }

    mvar->initialized = 0;
    mvar->emptySem = 0;
    mvar->fullSem = 0;
    mvar->value = 0;

    uint64_t empty = sysSemCreate(emptyName, 1);

    if (empty == 0) {
        return 0;
    }

    uint64_t full = sysSemCreate(fullName, 0);

    if (full == 0) {
        sysSemClose(empty);
        return 0;
    }

    mvar->emptySem = empty;
    mvar->fullSem = full;
    mvar->initialized = 1;

    return 1;
}

uint8_t mvarPut(MVar *mvar, uint64_t value) {
    if (mvar == 0 || !mvar->initialized) {
        return 0;
    }

    if (!sysSemWait(mvar->emptySem)) {
        return 0;
    }

    mvar->value = value;

    return sysSemPost(mvar->fullSem) != 0;
}

uint8_t mvarTake(MVar *mvar, uint64_t *value) {
    if (mvar == 0 || value == 0 || !mvar->initialized) {
        return 0;
    }

    if (!sysSemWait(mvar->fullSem)) {
        return 0;
    }

    *value = mvar->value;

    return sysSemPost(mvar->emptySem) != 0;
}

uint8_t mvarClose(MVar *mvar) {
    if (mvar == 0 || !mvar->initialized) {
        return 0;
    }

    uint8_t emptyClosed = sysSemClose(mvar->emptySem) != 0;
    uint8_t fullClosed = sysSemClose(mvar->fullSem) != 0;

    mvar->initialized = 0;
    mvar->emptySem = 0;
    mvar->fullSem = 0;

    return emptyClosed && fullClosed;
}
