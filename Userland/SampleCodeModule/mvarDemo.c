#include <stdint.h>
#include <syscalls.h>
#include <mvar.h>

#define MVAR_GROUPS 4
#define MVAR_MAX_WORKERS 8
#define MVAR_PS_CAPACITY 64

typedef struct {
    volatile uint8_t inUse;
    MVar box;
    uint64_t writerPid[MVAR_MAX_WORKERS];
    uint64_t readerPid[MVAR_MAX_WORKERS];
    uint64_t writers;
    uint64_t readers;
    ProcessInfo snapshot[MVAR_PS_CAPACITY];
} MVarGroup;

static MVarGroup groups[MVAR_GROUPS];

static void say(const char *s) {
    uint64_t n = 0;
    while (s[n] != '\0') n++;
    sysWrite(STDOUT, s, n);
}

static uint8_t parseCount(const char *s, uint64_t *result) {
    uint64_t value = 0;
    if (s == 0 || *s == '\0') return 0;
    for (; *s != '\0'; s++) {
        if (*s < '0' || *s > '9') return 0;
        value = value * 10 + (uint64_t)(*s - '0');
        if (value > MVAR_MAX_WORKERS) return 0;
    }
    if (value == 0) return 0;
    *result = value;
    return 1;
}

static void semName(char *dest, const char *prefix, uint64_t pid) {
    uint64_t n = 0;
    while (prefix[n] != '\0') { dest[n] = prefix[n]; n++; }
    uint64_t start = n;
    do {
        dest[n++] = (char)('0' + pid % 10);
        pid /= 10;
    } while (pid != 0);
    dest[n] = '\0';
    for (uint64_t a = start, b = n - 1; a < b; a++, b--) {
        char temp = dest[a]; dest[a] = dest[b]; dest[b] = temp;
    }
}

static MVarGroup *workerGroup(uint64_t argc, char **argv) {
    if (argc != 2 || argv == 0 || argv[0] == 0 ||
        argv[0][0] < '0' || argv[0][0] >= '0' + MVAR_GROUPS ||
        argv[0][1] != '\0') return 0;
    MVarGroup *g = &groups[argv[0][0] - '0'];
    return g->inUse ? g : 0;
}

/* Espera activa aleatoria: no usamos sysYield para simularla. */
static void randomBusyWait(uint64_t *seed) {
    *seed ^= *seed << 13;
    *seed ^= *seed >> 7;
    *seed ^= *seed << 17;
    uint64_t start = sysGetTicks();
    uint64_t duration = 15 + (*seed % 25);
    while (sysGetTicks() - start < duration) {
        __asm__ volatile ("" ::: "memory");
    }
}

static uint64_t initialSeed(void) {
    return (sysGetPid() + 1) * 0x9E3779B97F4A7C15ULL ^
           (sysGetTicks() + 1);
}

static void writer(uint64_t argc, char **argv) {
    MVarGroup *g = workerGroup(argc, argv);
    if (g == 0 || argv[1] == 0) return;
    uint64_t seed = initialSeed();
    uint64_t letter = (uint8_t)argv[1][0];
    for (;;) {
        randomBusyWait(&seed);
        if (!mvarPut(&g->box, letter)) return;
    }
}

static void reader(uint64_t argc, char **argv) {
    MVarGroup *g = workerGroup(argc, argv);
    if (g == 0 || argv[1] == 0) return;
    uint64_t seed = initialSeed();
    char message[] = "L0 recibio A\n";
    message[1] = argv[1][0];
    for (;;) {
        uint64_t value;
        randomBusyWait(&seed);
        if (!mvarTake(&g->box, &value)) return;
        message[11] = (char)value;
        sysWrite(STDOUT, message, 13);

        /* Dar tiempo para leer la salida y usar la shell. */
        uint64_t start = sysGetTicks();
        while (sysGetTicks() - start < 50) {
            __asm__ volatile ("" ::: "memory");
        }
    }
}

static uint8_t pidIsVisible(const MVarGroup *g, uint64_t pid,
                            uint64_t count) {
    for (uint64_t k = 0; k < count; k++) {
        if (g->snapshot[k].pid == pid) return 1;
    }
    return 0;
}

static void cleanupMonitor(uint64_t argc, char **argv) {
    if (argc != 1 || argv == 0 || argv[0] == 0 ||
        argv[0][0] < '0' || argv[0][0] >= '0' + MVAR_GROUPS ||
        argv[0][1] != '\0') return;
    MVarGroup *g = &groups[argv[0][0] - '0'];

    for (;;) {
        uint64_t count = sysPs(g->snapshot, MVAR_PS_CAPACITY);
        if (count > MVAR_PS_CAPACITY) count = MVAR_PS_CAPACITY;
        uint8_t any = 0;
        for (uint64_t i = 0; i < g->writers; i++) {
            if (pidIsVisible(g, g->writerPid[i], count)) any = 1;
        }
        for (uint64_t i = 0; i < g->readers; i++) {
            if (pidIsVisible(g, g->readerPid[i], count)) any = 1;
        }
        if (!any) break;
        for (uint64_t i = 0; i < 32; i++) sysYield();
    }

    if (!mvarClose(&g->box)) say("ERROR mvar: cierre de semaforos\n");
    g->inUse = 0;
}

void shellMvarProcess(uint64_t argc, char **argv) {
    uint64_t writers, readers;
    if (argc != 3 || argv == 0 ||
        !parseCount(argv[1], &writers) ||
        !parseCount(argv[2], &readers)) {
        say("Uso: mvar <escritores 1..8> <lectores 1..8>\n");
        return;
    }

    uint64_t groupIndex = MVAR_GROUPS;
    for (uint64_t i = 0; i < MVAR_GROUPS; i++) {
        if (__sync_bool_compare_and_swap(&groups[i].inUse, 0, 1)) {
            groupIndex = i;
            break;
        }
    }
    if (groupIndex == MVAR_GROUPS) {
        say("ERROR mvar: demasiadas sesiones activas\n");
        return;
    }

    MVarGroup *g = &groups[groupIndex];
    g->writers = writers;
    g->readers = readers;
    for (uint64_t i = 0; i < MVAR_MAX_WORKERS; i++) {
        g->writerPid[i] = 0;
        g->readerPid[i] = 0;
    }

    char empty[32], full[32];
    semName(empty, "mvar_e_", sysGetPid());
    semName(full, "mvar_f_", sysGetPid());
    if (!mvarInit(&g->box, empty, full)) {
        g->inUse = 0;
        say("ERROR mvar: no se pudo inicializar\n");
        return;
    }

    char groupArg[] = {'0' + (char)groupIndex, '\0'};
    char workerArg[] = {'0', '\0'};
    const char *args[] = {groupArg, workerArg};
    char nameReader[] = "mvar_reader_1";
    char nameWriter[] = "mvar_writer_A";

    for (uint64_t i = 0; i < readers; i++) {
        workerArg[0] = (char)('1' + i);
        nameReader[12] = workerArg[0];
        g->readerPid[i] = sysCreateProcess(nameReader, reader, 1, 0, 2, args);
        if (g->readerPid[i] == 0) goto error;
    }
    for (uint64_t i = 0; i < writers; i++) {
        workerArg[0] = (char)('A' + i);
        nameWriter[12] = workerArg[0];
        g->writerPid[i] = sysCreateProcess(nameWriter, writer, 1, 0, 2, args);
        if (g->writerPid[i] == 0) goto error;
    }

    const char *monitorArgs[] = {groupArg};
    if (sysCreateProcess("mvar_monitor", cleanupMonitor, 1, 0,
                         1, monitorArgs) == 0) goto error;

    say("mvar iniciado: usa ps, kill y nice sobre los hijos.\n");
    return;  /* La shell recupera el control sin esperar a los hijos. */

error:
    for (uint64_t i = 0; i < writers; i++)
        if (g->writerPid[i] != 0) sysKill(g->writerPid[i]);
    for (uint64_t i = 0; i < readers; i++)
        if (g->readerPid[i] != 0) sysKill(g->readerPid[i]);
    for (uint64_t i = 0; i < writers; i++)
        if (g->writerPid[i] != 0) sysWait(g->writerPid[i]);
    for (uint64_t i = 0; i < readers; i++)
        if (g->readerPid[i] != 0) sysWait(g->readerPid[i]);
    mvarClose(&g->box);
    g->inUse = 0;
    say("ERROR mvar: creacion de procesos\n");
}
