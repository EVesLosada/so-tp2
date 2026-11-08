#include <stdint.h>
#include <syscalls.h>
#include <shellPrograms.h>

#define TEST_PRIO_CAPACITY 64

static ProcessInfo snapshots[TEST_PRIO_CAPACITY];

static void testPrioPrint(const char *text) {
    uint64_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    sysWrite(STDOUT, text, length);
}

static uint8_t testPrioFind(uint64_t pid, ProcessInfo *result) {
    uint64_t count = sysPs(snapshots, TEST_PRIO_CAPACITY);

    if (count > TEST_PRIO_CAPACITY) {
        count = TEST_PRIO_CAPACITY;
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

static void testPrioWorker(uint64_t argc, char **argv) {
    (void)argc;
    (void)argv;

    for (;;) {
        sysYield();
    }
}

void shellTestPrioProcess(uint64_t argc, char **argv) {
    (void)argc;
    (void)argv;

    uint64_t pid = sysCreateProcess(
        "test_prio_worker",
        testPrioWorker,
        1,
        0,
        0,
        0
    );

    if (pid == 0) {
        testPrioPrint("ERROR test_prio: creacion\n");
        return;
    }

    ProcessInfo info;

    if (!testPrioFind(pid, &info) || info.priority != 1) {
        testPrioPrint("ERROR test_prio: prioridad inicial\n");
        sysKill(pid);
        sysWait(pid);
        return;
    }

    if (!sysNice(pid, 5)) {
        testPrioPrint("ERROR test_prio: nice a 5\n");
        sysKill(pid);
        sysWait(pid);
        return;
    }

    if (!testPrioFind(pid, &info) || info.priority != 5) {
        testPrioPrint("ERROR test_prio: prioridad 5\n");
        sysKill(pid);
        sysWait(pid);
        return;
    }

    if (!sysNice(pid, 2)) {
        testPrioPrint("ERROR test_prio: nice a 2\n");
        sysKill(pid);
        sysWait(pid);
        return;
    }

    if (!testPrioFind(pid, &info) || info.priority != 2) {
        testPrioPrint("ERROR test_prio: prioridad 2\n");
        sysKill(pid);
        sysWait(pid);
        return;
    }

    if (!sysKill(pid)) {
        testPrioPrint("ERROR test_prio: terminacion\n");
        return;
    }

    if (sysWait(pid) != pid) {
        testPrioPrint("ERROR test_prio: wait\n");
        return;
    }

    testPrioPrint("OK: test_prio superado\n");
}
