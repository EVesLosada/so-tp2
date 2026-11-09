#include <stdint.h>
#include <syscalls.h>
#include <shellPrograms.h>
#include <mvar.h>

#define SYNC_ITERATIONS 400
#define MVAR_ITERATIONS 64

static uint64_t mutexSem;
static volatile uint64_t sharedCounter;
static volatile uint8_t counterError;

static MVar sharedMvar;
static volatile uint8_t mvarError;

static void testSyncPrint(const char *text) {
    uint64_t length = 0;

    while (text[length] != '\0') {
        length++;
    }

    sysWrite(STDOUT, text, length);
}

/*
 * Dos procesos ejecutan esta funcion.
 * El yield dentro de la seccion critica obliga al
 * scheduler a probar la exclusion mutua.
 */
static void counterWorker(uint64_t argc, char **argv) {
    (void)argc;
    (void)argv;

    for (uint64_t i = 0; i < SYNC_ITERATIONS; i++) {
        if (!sysSemWait(mutexSem)) {
            counterError = 1;
            return;
        }

        uint64_t previous = sharedCounter;

        sysYield();

        sharedCounter = previous + 1;

        if (!sysSemPost(mutexSem)) {
            counterError = 1;
            return;
        }
    }
}

static uint8_t runSemaphoreTest(void) {
    sharedCounter = 0;
    counterError = 0;

    mutexSem = sysSemCreate("test_sync_mutex", 1);

    if (mutexSem == 0) {
        testSyncPrint("ERROR test_sync: crear semaforo\n");
        return 0;
    }

    uint64_t first = sysCreateProcess(
        "sync_counter_1",
        counterWorker,
        1,
        0,
        0,
        0
    );

    if (first == 0) {
        sysSemClose(mutexSem);
        testSyncPrint("ERROR test_sync: crear primer proceso\n");
        return 0;
    }

    uint64_t second = sysCreateProcess(
        "sync_counter_2",
        counterWorker,
        1,
        0,
        0,
        0
    );

    if (second == 0) {
        sysKill(first);
        sysWait(first);
        sysSemClose(mutexSem);
        testSyncPrint("ERROR test_sync: crear segundo proceso\n");
        return 0;
    }

    uint8_t firstWaitOk = sysWait(first) == first;
    uint8_t secondWaitOk = sysWait(second) == second;

    uint8_t result =
        firstWaitOk &&
        secondWaitOk &&
        !counterError &&
        sharedCounter == 2 * SYNC_ITERATIONS;

    if (!sysSemClose(mutexSem)) {
        result = 0;
    }

    if (result) {
        testSyncPrint("OK: test_sync semaforos\n");
    } else {
        testSyncPrint("ERROR test_sync: contador o sincronizacion\n");
    }

    return result;
}

/*
 * El productor envia valores consecutivos.
 */
static void mvarProducer(uint64_t argc, char **argv) {
    (void)argc;
    (void)argv;

    for (uint64_t i = 1; i <= MVAR_ITERATIONS; i++) {
        if (!mvarPut(&sharedMvar, i)) {
            mvarError = 1;
            return;
        }

        sysYield();
    }
}

/*
 * El consumidor verifica cada valor recibido.
 */
static void mvarConsumer(uint64_t argc, char **argv) {
    (void)argc;
    (void)argv;

    for (uint64_t expected = 1;
         expected <= MVAR_ITERATIONS;
         expected++) {

        uint64_t received = 0;

        if (!mvarTake(&sharedMvar, &received)) {
            mvarError = 1;
            return;
        }

        if (received != expected) {
            mvarError = 1;
            return;
        }

        sysYield();
    }
}

static uint8_t runMvarTest(void) {
    mvarError = 0;

    if (!mvarInit(
            &sharedMvar,
            "test_sync_empty",
            "test_sync_full")) {
        testSyncPrint("ERROR test_sync: inicializar MVar\n");
        return 0;
    }

    uint64_t consumer = sysCreateProcess(
        "mvar_consumer",
        mvarConsumer,
        1,
        0,
        0,
        0
    );

    if (consumer == 0) {
        mvarClose(&sharedMvar);
        testSyncPrint("ERROR test_sync: crear consumidor\n");
        return 0;
    }

    /*
     * Le damos oportunidad al consumidor de intentar
     * leer el MVar vacio y bloquearse.
     */
    for (uint64_t i = 0; i < 10; i++) {
        sysYield();
    }

    uint64_t producer = sysCreateProcess(
        "mvar_producer",
        mvarProducer,
        1,
        0,
        0,
        0
    );

    if (producer == 0) {
        sysKill(consumer);
        sysWait(consumer);
        mvarClose(&sharedMvar);
        testSyncPrint("ERROR test_sync: crear productor\n");
        return 0;
    }

    uint8_t producerWaitOk = sysWait(producer) == producer;
    uint8_t consumerWaitOk = sysWait(consumer) == consumer;

    uint8_t result =
        producerWaitOk &&
        consumerWaitOk &&
        !mvarError;

    if (!mvarClose(&sharedMvar)) {
        result = 0;
    }

    if (result) {
        testSyncPrint("OK: test_sync MVar\n");
    } else {
        testSyncPrint("ERROR test_sync: MVar\n");
    }

    return result;
}

/*
 * Comando de la shell.
 */
uint8_t runSyncPairs(
    uint64_t pairs,
    uint8_t withSemaphores,
    uint64_t iterations
);

static uint8_t parseSyncNumber(
    const char *text,
    uint64_t limit,
    uint64_t *result
) {
    uint64_t value = 0;

    if (text == 0 || text[0] == '\0' || result == 0) {
        return 0;
    }

    for (uint64_t i = 0; text[i] != '\0'; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return 0;
        }

        uint64_t digit = text[i] - '0';

        if (digit > limit ||
            value > (limit - digit) / 10) {
            return 0;
        }

        value = value * 10 + digit;
    }

    *result = value;
    return 1;
}

void shellTestSyncProcess(uint64_t argc, char **argv) {
    if (argc == 1) {
        testSyncPrint("Iniciando test_sync...\n");

        if (!runSemaphoreTest() || !runMvarTest()) {
            return;
        }

        testSyncPrint("OK: test_sync superado\n");
        return;
    }

    uint64_t pairs;
    uint64_t useSem;
    uint64_t iterations;

    if (argc != 4 || argv == 0 ||
        !parseSyncNumber(argv[1], 8, &pairs) ||
        !parseSyncNumber(argv[2], 50000, &iterations) ||
        !parseSyncNumber(argv[3], 1, &useSem) ||
        pairs == 0 || iterations == 0) {
        testSyncPrint(
            "Uso: test_sync <pares 1..8> <iteraciones 1..50000> <semaforos 0|1>\n"
        );
        return;
    }

    if (runSyncPairs(pairs, (uint8_t)useSem, iterations)) {
        testSyncPrint("OK: test_sync superado\n");
    }
}
