#include <stdint.h>
#include <syscalls.h>
#include <shellPrograms.h>

#define TEST_PROC_CAPACITY 64

static ProcessInfo snapshots[TEST_PROC_CAPACITY];

static void testProcPrint(const char *text) {
    uint64_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    sysWrite(STDOUT, text, length);
}

static uint8_t testProcFind(uint64_t pid, ProcessInfo *result) {
    uint64_t count = sysPs(snapshots, TEST_PROC_CAPACITY);

    if (count > TEST_PROC_CAPACITY) {
        count = TEST_PROC_CAPACITY;
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

static void testProcWorker(uint64_t argc, char **argv) {
    (void)argc;
    (void)argv;

    for (;;) {
        sysYield();
    }
}

void shellTestProcProcess(uint64_t argc, char **argv) {
    (void)argc;
    (void)argv;

    uint64_t pid = sysCreateProcess(
        "test_proc_worker",
        testProcWorker,
        1,
        0,
        0,
        0
    );

    if (pid == 0) {
        testProcPrint("ERROR test_proc: creacion\n");
        return;
    }

    ProcessInfo info;

    if (!testProcFind(pid, &info) ||
        (info.state != PROCESS_READY &&
         info.state != PROCESS_RUNNING)) {
        testProcPrint("ERROR test_proc: proceso no visible\n");
        sysKill(pid);
        sysWait(pid);
        return;
    }

    if (!sysBlock(pid)) {
        testProcPrint("ERROR test_proc: bloqueo\n");
        sysKill(pid);
        sysWait(pid);
        return;
    }

    if (!testProcFind(pid, &info) ||
        info.state != PROCESS_BLOCKED) {
        testProcPrint("ERROR test_proc: estado BLOCKED\n");
        sysKill(pid);
        sysWait(pid);
        return;
    }

    if (!sysUnblock(pid)) {
        testProcPrint("ERROR test_proc: desbloqueo\n");
        sysKill(pid);
        sysWait(pid);
        return;
    }

    if (!testProcFind(pid, &info) ||
        (info.state != PROCESS_READY &&
         info.state != PROCESS_RUNNING)) {
        testProcPrint("ERROR test_proc: estado tras desbloqueo\n");
        sysKill(pid);
        sysWait(pid);
        return;
    }

    if (!sysKill(pid)) {
        testProcPrint("ERROR test_proc: terminacion\n");
        return;
    }

    if (sysWait(pid) != pid) {
        testProcPrint("ERROR test_proc: wait\n");
        return;
    }

    if (testProcFind(pid, 0)) {
        testProcPrint("ERROR test_proc: proceso no liberado\n");
        return;
    }

    testProcPrint("OK: test_proc superado\n");
}
