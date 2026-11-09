#include <stdint.h>
#include <syscalls.h>
#include <shellPrograms.h>

#define TEST_PRIO_WORKERS 3
#define TEST_PRIO_MAX_TARGET 100000000ULL

static volatile uint64_t counters[TEST_PRIO_WORKERS];
static volatile uint64_t finishTicks[TEST_PRIO_WORKERS];

static void printText(const char *text) {
    uint64_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    sysWrite(STDOUT, text, length);
}

static void printNumber(uint64_t number) {
    char buffer[21];
    uint64_t length = 0;

    do {
        buffer[length++] = '0' + (number % 10);
        number /= 10;
    } while (number != 0);

    for (uint64_t i = 0; i < length / 2; i++) {
        char temp = buffer[i];
        buffer[i] = buffer[length - 1 - i];
        buffer[length - 1 - i] = temp;
    }

    sysWrite(STDOUT, buffer, length);
}

static uint64_t parseTarget(const char *text) {
    uint64_t value = 0;

    if (text == 0 || *text == '\0') {
        return 0;
    }

    while (*text != '\0') {
        if (*text < '0' || *text > '9') {
            return 0;
        }

        uint64_t digit = (uint64_t)(*text - '0');

        if (value > (TEST_PRIO_MAX_TARGET - digit) / 10) {
            return 0;
        }

        value = value * 10 + digit;
        text++;
    }

    return value;
}

static void counterWorker(uint64_t argc, char **argv) {
    if (argc != 2 || argv == 0) {
        return;
    }

    uint64_t index = parseTarget(argv[0]);
    uint64_t target = parseTarget(argv[1]);

    if (index >= TEST_PRIO_WORKERS || target == 0) {
        return;
    }

    while (counters[index] < target) {
        counters[index]++;
    }

    finishTicks[index] = sysGetTicks();
}

static uint8_t runScenario(
    uint64_t target,
    const char *targetText,
    const uint64_t priorities[TEST_PRIO_WORKERS],
    const char *label
) {
    uint64_t pids[TEST_PRIO_WORKERS] = {0};
    const char *indices[TEST_PRIO_WORKERS] = {"0", "1", "2"};

    uint64_t startTick = sysGetTicks();

    for (uint64_t i = 0; i < TEST_PRIO_WORKERS; i++) {
        counters[i] = 0;
        finishTicks[i] = 0;
    }

    for (uint64_t i = 0; i < TEST_PRIO_WORKERS; i++) {
        const char *args[2] = {indices[i], targetText};

        pids[i] = sysCreateProcess(
            "test_prio_worker",
            counterWorker,
            priorities[i],
            0,
            2,
            args
        );

        if (pids[i] == 0) {
            printText("ERROR test_prio: crear proceso\n");
            goto cleanup;
        }
    }

    for (uint64_t i = 0; i < TEST_PRIO_WORKERS; i++) {
        if (sysWait(pids[i]) != pids[i]) {
            printText("ERROR test_prio: esperar proceso\n");
            goto cleanup;
        }
        pids[i] = 0;
    }

    printText(label);
    printText("\n");

    for (uint64_t i = 0; i < TEST_PRIO_WORKERS; i++) {
        if (counters[i] != target || finishTicks[i] == 0) {
            printText("ERROR test_prio: contador incompleto\n");
            return 0;
        }

        printText("  Proceso ");
        printNumber(i + 1);
        printText(" | prioridad ");
        printNumber(priorities[i]);
        printText(" | contador ");
        printNumber(counters[i]);
        printText(" | ticks ");
        printNumber(finishTicks[i] - startTick);
        printText("\n");
    }

    return 1;

cleanup:
    for (uint64_t i = 0; i < TEST_PRIO_WORKERS; i++) {
        if (pids[i] != 0) {
            sysKill(pids[i]);
            sysWait(pids[i]);
        }
    }
    return 0;
}

void shellTestPrioProcess(uint64_t argc, char **argv) {
    if (argc != 2) {
        printText("Uso: test_prio <objetivo>\n");
        return;
    }

    uint64_t target = parseTarget(argv[1]);

    if (target == 0) {
        printText("ERROR test_prio: objetivo invalido\n");
        return;
    }

    const uint64_t equal[TEST_PRIO_WORKERS] = {3, 3, 3};
    const uint64_t different[TEST_PRIO_WORKERS] = {1, 3, 5};

    if (!runScenario(target, argv[1], equal,
                     "Escenario 1: prioridades iguales")) {
        return;
    }

    if (!runScenario(target, argv[1], different,
                     "Escenario 2: prioridades diferentes")) {
        return;
    }

    printText("OK: test_prio superado\n");
}
