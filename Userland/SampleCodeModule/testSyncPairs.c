#include <stdint.h>
#include <syscalls.h>

#define MAX_SYNC_PAIRS 8
#define MAX_SYNC_WORKERS (2 * MAX_SYNC_PAIRS)
#define MAX_SYNC_SESSIONS 8
#define SYNC_PS_CAPACITY 64

typedef struct {
    volatile uint8_t inUse;
    volatile int64_t sharedValue;
    volatile uint8_t workerError;
    uint64_t iterations;
    uint64_t mutexId;
    uint64_t parentPid;
    uint64_t childPids[MAX_SYNC_WORKERS];
    uint8_t useMutex;
    ProcessInfo snapshot[SYNC_PS_CAPACITY];
} SyncSession;

/* Cada instancia de test_sync usa su propia sesion. */
static SyncSession sessions[MAX_SYNC_SESSIONS];

static void printText(const char *text) {
    uint64_t size = 0;
    while (text[size] != '\0') size++;
    sysWrite(STDOUT, text, size);
}

static void printNumber(int64_t number) {
    char buffer[24];
    uint64_t size = 0;
    uint64_t value;

    if (number < 0) {
        sysWrite(STDOUT, "-", 1);
        value = (uint64_t)(-(number + 1)) + 1;
    } else {
        value = (uint64_t)number;
    }

    do {
        buffer[size++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);

    while (size > 0) {
        size--;
        sysWrite(STDOUT, &buffer[size], 1);
    }
}

static uint8_t pidVisible(uint64_t pid, const ProcessInfo *snapshot,
                          uint64_t count) {
    if (pid == 0) return 0;
    for (uint64_t i = 0; i < count; i++) {
        if (snapshot[i].pid == pid) return 1;
    }
    return 0;
}

/*
 * Si Ctrl+C termino al padre y a todos los hijos, una ejecucion
 * posterior puede recuperar la sesion abandonada sin tocar el kernel.
 */
static void reclaimAbandonedSessions(void) {
    ProcessInfo *snapshot = sysMalloc(sizeof(ProcessInfo) * SYNC_PS_CAPACITY);
    if (snapshot == 0) return;

    for (uint64_t i = 0; i < MAX_SYNC_SESSIONS; i++) {
        SyncSession *s = &sessions[i];
        if (s->inUse != 1 || s->parentPid == 0) continue;

        uint64_t count = sysPs(snapshot, SYNC_PS_CAPACITY);
        if (count > SYNC_PS_CAPACITY) count = SYNC_PS_CAPACITY;
        if (pidVisible(s->parentPid, snapshot, count)) continue;

        uint8_t childAlive = 0;
        for (uint64_t j = 0; j < MAX_SYNC_WORKERS; j++) {
            if (pidVisible(s->childPids[j], snapshot, count)) {
                childAlive = 1;
                break;
            }
        }
        if (childAlive) continue;

        if (!__sync_bool_compare_and_swap(&s->inUse, 1, 2)) continue;
        if (s->mutexId != 0 && !sysSemClose(s->mutexId)) {
            s->inUse = 1;
            continue;
        }
        s->mutexId = 0;
        s->parentPid = 0;
        s->inUse = 0;
    }

    sysFree(snapshot);
}

static SyncSession *findSession(uint64_t argc, char **argv) {
    if (argc != 1 || argv == 0 || argv[0] == 0 ||
        argv[0][0] < '0' ||
        argv[0][0] >= '0' + MAX_SYNC_SESSIONS ||
        argv[0][1] != '\0') return 0;

    SyncSession *s = &sessions[argv[0][0] - '0'];
    return s->inUse == 1 ? s : 0;
}

static void counterWorker(uint64_t argc, char **argv, int64_t delta) {
    SyncSession *s = findSession(argc, argv);
    if (s == 0) return;

    for (uint64_t i = 0; i < s->iterations; i++) {
        if (s->useMutex && !sysSemWait(s->mutexId)) {
            s->workerError = 1;
            return;
        }

        int64_t previous = s->sharedValue;

        if (!s->useMutex) {
            sysYield();
        }

        s->sharedValue = previous + delta;

        if (s->useMutex && !sysSemPost(s->mutexId)) {
            s->workerError = 1;
            return;
        }
    }
}

static void incrementWorker(uint64_t argc, char **argv) {
    counterWorker(argc, argv, 1);
}

static void decrementWorker(uint64_t argc, char **argv) {
    counterWorker(argc, argv, -1);
}

uint8_t runSyncPairs(uint64_t pairs, uint8_t withSemaphores,
                     uint64_t iterations) {
    if (pairs == 0 || pairs > MAX_SYNC_PAIRS || iterations == 0) return 0;

    reclaimAbandonedSessions();

    uint64_t slot = MAX_SYNC_SESSIONS;
    for (uint64_t i = 0; i < MAX_SYNC_SESSIONS; i++) {
        if (__sync_bool_compare_and_swap(&sessions[i].inUse, 0, 1)) {
            slot = i;
            break;
        }
    }
    if (slot == MAX_SYNC_SESSIONS) {
        printText("ERROR test_sync: demasiadas sesiones simultaneas\n");
        return 0;
    }

    SyncSession *s = &sessions[slot];
    s->parentPid = sysGetPid();
    s->sharedValue = 0;
    s->workerError = 0;
    s->iterations = iterations;
    s->useMutex = withSemaphores != 0;
    s->mutexId = 0;
    for (uint64_t i = 0; i < MAX_SYNC_WORKERS; i++) s->childPids[i] = 0;

    uint64_t count = sysPs(s->snapshot, SYNC_PS_CAPACITY);
    if (count > SYNC_PS_CAPACITY) count = SYNC_PS_CAPACITY;
    uint8_t foreground = 0;
    uint8_t foundParent = 0;
    for (uint64_t i = 0; i < count; i++) {
        if (s->snapshot[i].pid == s->parentPid) {
            foreground = s->snapshot[i].foreground;
            foundParent = 1;
            break;
        }
    }
    if (!foundParent) {
        printText("ERROR test_sync: padre no encontrado\n");
        goto cleanup;
    }

    if (s->useMutex) {
        char name[] = "test_sync_mutex_0";
        name[16] = (char)('0' + slot);
        s->mutexId = sysSemCreate(name, 1);
        if (s->mutexId == 0) {
            printText("ERROR test_sync: crear semaforo\n");
            goto cleanup;
        }
    }

    char slotArg[] = {'0', '\0'};
    slotArg[0] = (char)('0' + slot);
    const char *workerArgs[] = {slotArg};
    uint64_t created = 0;

    for (uint64_t i = 0; i < pairs; i++) {
        uint64_t pid = sysCreateProcess("sync_increment", incrementWorker,
                                        1, foreground, 1, workerArgs);
        if (pid == 0) {
            printText("ERROR test_sync: crear incrementador\n");
            goto cleanup;
        }
        s->childPids[created++] = pid;

        pid = sysCreateProcess("sync_decrement", decrementWorker,
                               1, foreground, 1, workerArgs);
        if (pid == 0) {
            printText("ERROR test_sync: crear decrementador\n");
            goto cleanup;
        }
        s->childPids[created++] = pid;
    }

    for (uint64_t i = 0; i < created; i++) {
        uint64_t pid = s->childPids[i];
        if (sysWait(pid) != pid) {
            printText("ERROR test_sync: esperar hijo\n");
            goto cleanup;
        }
        s->childPids[i] = 0;
    }

    if (s->workerError) {
        printText("ERROR test_sync: fallo de semaforo\n");
        goto cleanup;
    }

    printText("Contador final: ");
    printNumber(s->sharedValue);
    printText("\n");

    if (s->useMutex && s->sharedValue != 0) {
        printText("ERROR test_sync: contador distinto de cero\n");
        goto cleanup;
    }

    if (!s->useMutex) {
        printText("INFO: sin semaforos puede haber race conditions\n");
    }

    if (s->mutexId != 0 && !sysSemClose(s->mutexId)) {
        printText("ERROR test_sync: cerrar semaforo\n");
        return 0;
    }
    s->mutexId = 0;
    s->parentPid = 0;
    s->inUse = 0;
    printText("OK: test_sync pares completado\n");
    return 1;

cleanup:
    /* Terminar primero a todos, y luego esperarlos. */
    for (uint64_t i = 0; i < MAX_SYNC_WORKERS; i++) {
        if (s->childPids[i] != 0) sysKill(s->childPids[i]);
    }
    for (uint64_t i = 0; i < MAX_SYNC_WORKERS; i++) {
        if (s->childPids[i] != 0) {
            sysWait(s->childPids[i]);
            s->childPids[i] = 0;
        }
    }
    if (s->mutexId != 0 && !sysSemClose(s->mutexId)) {
        printText("ERROR test_sync: cerrar semaforo\n");
        return 0;
    }
    s->mutexId = 0;
    s->parentPid = 0;
    s->inUse = 0;
    return 0;
}
