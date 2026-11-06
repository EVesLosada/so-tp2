#include <stdint.h>
#include <stddef.h>
#include <pipe.h>
#include <ioDescriptor.h>
#include <criticalSection.h>

static Pipe pipeTable[MAX_PIPES];

static uint64_t nextPipeId = 1;

static uint8_t pipeNamesEqual(
    const char *first,
    const char *second
) {
    if (first == NULL || second == NULL) {
        return 0;
    }

    for (uint64_t i = 0; i < PIPE_NAME_MAX_LENGTH; i++) {
        if (first[i] != second[i]) {
            return 0;
        }

        if (first[i] == '\0') {
            return 1;
        }
    }

    return 0;
}

static Pipe *findPipeByName(const char *name) {
    for (uint64_t i = 0; i < MAX_PIPES; i++) {
        if (pipeTable[i].inUse &&
            pipeNamesEqual(pipeTable[i].name, name)) {
            return &pipeTable[i];
        }
    }

    return NULL;
}

uint64_t pipeCreate(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return 0;
    }

    uint64_t length = 0;

    while (length < PIPE_NAME_MAX_LENGTH &&
           name[length] != '\0') {
        length++;
    }

    if (length == PIPE_NAME_MAX_LENGTH) {
        return 0;
    }

    uint64_t flags = enterCriticalSection();

    if (findPipeByName(name) != NULL) {
        leaveCriticalSection(flags);
        return 0;
    }

    for (uint64_t i = 0; i < MAX_PIPES; i++) {
        Pipe *pipe = &pipeTable[i];

        if (pipe->inUse) {
            continue;
        }

        *pipe = (Pipe){0};

        pipe->id = nextPipeId++;
        pipe->inUse = 1;

        for (uint64_t j = 0; j <= length; j++) {
            pipe->name[j] = name[j];
        }

        uint64_t id = pipe->id;

        leaveCriticalSection(flags);
        return id;
    }

    leaveCriticalSection(flags);
    return 0;
}

uint64_t pipeOpen(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return 0;
    }

    uint64_t flags = enterCriticalSection();

    Pipe *pipe = findPipeByName(name);
    uint64_t id = pipe == NULL ? 0 : pipe->id;

    leaveCriticalSection(flags);

    return id;
}


uint8_t pipeDestroy(uint64_t pipeId) {
    if (pipeId == 0) {
        return 0;
    }

    uint64_t flags = enterCriticalSection();

    Pipe *pipe = findPipeById(pipeId);

    if (pipe == NULL ||
        pipe->readers != 0 ||
        pipe->writers != 0 ||
        pipe->readerWaiters.count != 0 ||
        pipe->writerWaiters.count != 0) {
        leaveCriticalSection(flags);
        return 0;
    }

    *pipe = (Pipe){0};

    leaveCriticalSection(flags);
    return 1;
}

void initializePipeTable(void) {
    uint64_t flags = enterCriticalSection();

    for (uint64_t i = 0; i < MAX_PIPES; i++) {
        pipeTable[i] = (Pipe){0};
    }

    nextPipeId = 1;

    leaveCriticalSection(flags);
}

Pipe *findPipeById(uint64_t id) {
    if (id == 0) {
        return NULL;
    }

    for (uint64_t i = 0; i < MAX_PIPES; i++) {
        if (pipeTable[i].inUse &&
            pipeTable[i].id == id) {
            return &pipeTable[i];
        }
    }

    return NULL;
}

uint64_t pipeBufferWrite(
    Pipe *pipe,
    const uint8_t *data,
    uint64_t count
) {
    if (pipe == NULL || data == NULL || !pipe->inUse) {
        return 0;
    }

    uint64_t written = 0;

    while (written < count &&
           pipe->count < PIPE_BUFFER_SIZE) {

        pipe->buffer[pipe->writeIndex] = data[written];

        pipe->writeIndex =
            (pipe->writeIndex + 1) % PIPE_BUFFER_SIZE;

        pipe->count++;
        written++;
    }

    return written;
}

uint64_t pipeBufferRead(
    Pipe *pipe,
    uint8_t *destination,
    uint64_t count
) {
    if (pipe == NULL ||
        destination == NULL ||
        !pipe->inUse) {
        return 0;
    }

    uint64_t read = 0;

    while (read < count && pipe->count > 0) {
        destination[read] = pipe->buffer[pipe->readIndex];

        pipe->readIndex =
            (pipe->readIndex + 1) % PIPE_BUFFER_SIZE;

        pipe->count--;
        read++;
    }

    return read;
}

/*
 * Asocia un extremo de pipe a un descriptor del proceso actual.
 *
 * La busqueda, asignacion y actualizacion de referencias
 * ocurren dentro de la misma seccion critica.
 */
uint64_t pipeOpenDescriptor(const char *name, PipeMode mode) {
    if (name == NULL ||
        (mode != PIPE_READ && mode != PIPE_WRITE)) {
        return MAX_PROCESS_DESCRIPTORS;
    }

    uint64_t flags = enterCriticalSection();

    Pipe *pipe = findPipeByName(name);

    if (pipe == NULL) {
        leaveCriticalSection(flags);
        return MAX_PROCESS_DESCRIPTORS;
    }

    IODescriptor descriptor = {
        .inUse = 1,
        .permissions = mode == PIPE_READ
            ? IO_ACCESS_READ
            : IO_ACCESS_WRITE,
        .type = IO_DESCRIPTOR_PIPE,
        .resourceId = pipe->id
    };

    IODescriptor *table = ioGetCurrentDescriptorTable();

    uint64_t fd = ioAllocateDescriptor(table, descriptor);

    if (fd == MAX_PROCESS_DESCRIPTORS) {
        leaveCriticalSection(flags);
        return MAX_PROCESS_DESCRIPTORS;
    }

    if (mode == PIPE_READ) {
        pipe->readers++;
    } else {
        pipe->writers++;
    }

    leaveCriticalSection(flags);

    return fd;
}

uint8_t pipeReleaseEndpoint(uint64_t pipeId, PipeMode mode) {
    if (pipeId == 0 ||
        (mode != PIPE_READ && mode != PIPE_WRITE)) {
        return 0;
    }

    uint64_t flags = enterCriticalSection();

    Pipe *pipe = findPipeById(pipeId);

    if (pipe == NULL) {
        leaveCriticalSection(flags);
        return 0;
    }

    if (mode == PIPE_READ) {
        if (pipe->readers == 0) {
            leaveCriticalSection(flags);
            return 0;
        }

        pipe->readers--;
    } else {
        if (pipe->writers == 0) {
            leaveCriticalSection(flags);
            return 0;
        }

        pipe->writers--;
    }

    leaveCriticalSection(flags);
    return 1;
}


/*
 * Selecciona la cola correspondiente al extremo.
 * Debe utilizarse dentro de una seccion critica.
 */
static PipeWaitQueue *getPipeWaitQueue(
    Pipe *pipe,
    PipeMode mode
) {
    if (pipe == NULL || !pipe->inUse) {
        return NULL;
    }

    if (mode == PIPE_READ) {
        return &pipe->readerWaiters;
    }

    if (mode == PIPE_WRITE) {
        return &pipe->writerWaiters;
    }

    return NULL;
}

/*
 * Agrega un PID al final de la cola FIFO.
 * Rechaza PIDs nulos, duplicados y colas llenas.
 */
uint8_t pipeEnqueueWaiter(
    Pipe *pipe,
    PipeMode mode,
    uint64_t pid
) {
    if (pid == 0) {
        return 0;
    }

    uint64_t flags = enterCriticalSection();
    PipeWaitQueue *queue = getPipeWaitQueue(pipe, mode);

    if (queue == NULL || queue->count >= PIPE_MAX_WAITERS) {
        leaveCriticalSection(flags);
        return 0;
    }

    for (uint64_t i = 0; i < queue->count; i++) {
        if (queue->pids[i] == pid) {
            leaveCriticalSection(flags);
            return 0;
        }
    }

    queue->pids[queue->count++] = pid;

    leaveCriticalSection(flags);
    return 1;
}

/*
 * Retira el primer PID de la cola.
 * Devuelve 0 si la cola esta vacia o es invalida.
 */
uint64_t pipeDequeueWaiter(
    Pipe *pipe,
    PipeMode mode
) {
    uint64_t flags = enterCriticalSection();
    PipeWaitQueue *queue = getPipeWaitQueue(pipe, mode);

    if (queue == NULL || queue->count == 0) {
        leaveCriticalSection(flags);
        return 0;
    }

    uint64_t pid = queue->pids[0];

    for (uint64_t i = 1; i < queue->count; i++) {
        queue->pids[i - 1] = queue->pids[i];
    }

    queue->count--;
    queue->pids[queue->count] = 0;

    leaveCriticalSection(flags);
    return pid;
}

/*
 * Elimina un PID especifico conservando el orden FIFO.
 * Sera util para cancelar esperas de procesos terminados.
 */
uint8_t pipeRemoveWaiter(
    Pipe *pipe,
    PipeMode mode,
    uint64_t pid
) {
    if (pid == 0) {
        return 0;
    }

    uint64_t flags = enterCriticalSection();
    PipeWaitQueue *queue = getPipeWaitQueue(pipe, mode);

    if (queue == NULL) {
        leaveCriticalSection(flags);
        return 0;
    }

    for (uint64_t i = 0; i < queue->count; i++) {
        if (queue->pids[i] != pid) {
            continue;
        }

        for (uint64_t j = i + 1; j < queue->count; j++) {
            queue->pids[j - 1] = queue->pids[j];
        }

        queue->count--;
        queue->pids[queue->count] = 0;

        leaveCriticalSection(flags);
        return 1;
    }

    leaveCriticalSection(flags);
    return 0;
}
