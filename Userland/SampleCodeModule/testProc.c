#include <stdint.h>
#include <syscalls.h>
#include <shellPrograms.h>

#define TEST_PROC_MAX 16
#define TEST_PROC_PS_CAPACITY 64

typedef struct {
    uint64_t pid;
    uint8_t blocked;
} DummyProcess;

static DummyProcess children[TEST_PROC_MAX];
static ProcessInfo snapshots[TEST_PROC_PS_CAPACITY];

static uint64_t randomState = 1;

static void printError(const char *message) {
    uint64_t length = 0;
    while (message[length] != '\0') {
        length++;
    }
    sysWrite(STDOUT, message, length);
}

static uint64_t randomNumber(void) {
    randomState ^= randomState << 13;
    randomState ^= randomState >> 7;
    randomState ^= randomState << 17;
    return randomState;
}

static uint64_t parsePositive(const char *text) {
    uint64_t value = 0;

    if (text == 0 || *text == '\0') {
        return 0;
    }

    while (*text != '\0') {
        if (*text < '0' || *text > '9') {
            return 0;
        }

        uint64_t digit = (uint64_t)(*text - '0');
        if (value > (TEST_PROC_MAX - digit) / 10) {
            return 0;
        }

        value = value * 10 + digit;
        text++;
    }

    return value;
}

static uint8_t findProcess(uint64_t pid, ProcessInfo *result) {
    uint64_t count = sysPs(snapshots, TEST_PROC_PS_CAPACITY);

    if (count > TEST_PROC_PS_CAPACITY) {
        count = TEST_PROC_PS_CAPACITY;
    }

    for (uint64_t i = 0; i < count; i++) {
        if (snapshots[i].pid == pid) {
            if (result != 0) {
                *result = snapshots[i];
            }
            return 1;
        }
    }

    return 0;
}

static void dummyWorker(uint64_t argc, char **argv) {
    (void)argc;
    (void)argv;

    for (;;) {
        sysYield();
    }
}

void shellTestProcProcess(uint64_t argc, char **argv) {
    if (argc != 2) {
        printError("ERROR test_proc: uso test_proc <1..16>\n");
        return;
    }

    uint64_t maximum = parsePositive(argv[1]);

    if (maximum == 0 || maximum > TEST_PROC_MAX) {
        printError("ERROR test_proc: cantidad invalida\n");
        return;
    }

    randomState = sysGetTicks() ^ sysGetPid() ^ 0x9E3779B97F4A7C15ULL;

    if (randomState == 0) {
        randomState = 1;
    }

    uint64_t active = 0;

    for (uint64_t i = 0; i < TEST_PROC_MAX; i++) {
        children[i].pid = 0;
        children[i].blocked = 0;
    }

    for (;;) {
        uint64_t operation = randomNumber() % 4;

        if (operation == 0 && active < maximum) {
            uint64_t slot = randomNumber() % maximum;

            if (children[slot].pid != 0) {
                continue;
            }

            uint64_t pid = sysCreateProcess(
                "test_proc_dummy",
                dummyWorker,
                1,
                1,
                0,
                0
            );

            if (pid == 0) {
                printError("ERROR test_proc: crear proceso\n");
                break;
            }

            children[slot].pid = pid;
            children[slot].blocked = 0;
            active++;
        } else if (operation != 0 && active > 0) {
            uint64_t slot = randomNumber() % maximum;

            if (children[slot].pid == 0) {
                continue;
            }

            uint64_t pid = children[slot].pid;
            ProcessInfo info;

            if (operation == 1 && !children[slot].blocked) {
                if (!sysBlock(pid)) {
                    printError("ERROR test_proc: bloquear\n");
                    break;
                }

                if (!findProcess(pid, &info) ||
                    info.state != PROCESS_BLOCKED) {
                    printError("ERROR test_proc: estado bloqueado\n");
                    break;
                }

                children[slot].blocked = 1;
            } else if (operation == 2 && children[slot].blocked) {
                if (!sysUnblock(pid)) {
                    printError("ERROR test_proc: desbloquear\n");
                    break;
                }

                if (!findProcess(pid, &info) ||
                    (info.state != PROCESS_READY &&
                     info.state != PROCESS_RUNNING)) {
                    printError("ERROR test_proc: estado desbloqueado\n");
                    break;
                }

                children[slot].blocked = 0;
            } else if (operation == 3) {
                if (!sysKill(pid)) {
                    printError("ERROR test_proc: matar\n");
                    break;
                }

                if (sysWait(pid) != pid) {
                    printError("ERROR test_proc: esperar hijo\n");
                    break;
                }

                children[slot].pid = 0;
                children[slot].blocked = 0;
                active--;
            }
        }

        sysYield();
    }

    /* Limpieza si una operacion falla. */
    for (uint64_t i = 0; i < maximum; i++) {
        if (children[i].pid != 0) {
            sysKill(children[i].pid);
            sysWait(children[i].pid);
            children[i].pid = 0;
        }
    }
}
