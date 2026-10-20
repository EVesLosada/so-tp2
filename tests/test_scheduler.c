#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <processManager.h>
#include <processTable.h>

#define SHELL_RSP 0x1000ULL
#define PROCESS_A_RSP 0x2000ULL
#define PROCESS_B_RSP 0x3000ULL

static void testSchedule(void) {
    initializeProcessManager();

    Process processA = {0};
    Process processB = {0};

    processA.pid = 1;
    processA.state = PROCESS_READY;
    processA.savedStackPointer = PROCESS_A_RSP;

    processB.pid = 2;
    processB.state = PROCESS_READY;
    processB.savedStackPointer = PROCESS_B_RSP;

    assert(registerProcess(&processA));
    assert(registerProcess(&processB));

    // La shell inicia el primer proceso.
    assert(schedule(SHELL_RSP) == PROCESS_A_RSP);
    assert(getCurrentProcess() == &processA);
    assert(processA.state == PROCESS_RUNNING);

    // El proceso A vuelve a la shell.
    assert(schedule(PROCESS_A_RSP) == SHELL_RSP);
    assert(getCurrentProcess() == NULL);
    assert(processA.state == PROCESS_READY);

    // La shell selecciona el proceso B.
    assert(schedule(SHELL_RSP) == PROCESS_B_RSP);
    assert(getCurrentProcess() == &processB);
    assert(processB.state == PROCESS_RUNNING);

    // El proceso B vuelve a la shell.
    assert(schedule(PROCESS_B_RSP) == SHELL_RSP);
    assert(getCurrentProcess() == NULL);
    assert(processB.state == PROCESS_READY);

    // Round Robin: vuelve a seleccionar A.
    assert(schedule(SHELL_RSP) == PROCESS_A_RSP);
    assert(getCurrentProcess() == &processA);

    printf("OK: alternancia shell y Round Robin\n");
}

static void testBlockedAndTerminated(void) {
    initializeProcessManager();

    Process processA = {0};
    Process processB = {0};

    processA.pid = 1;
    processA.state = PROCESS_READY;
    processA.savedStackPointer = PROCESS_A_RSP;

    processB.pid = 2;
    processB.state = PROCESS_READY;
    processB.savedStackPointer = PROCESS_B_RSP;

    assert(registerProcess(&processA));
    assert(registerProcess(&processB));

    // Iniciamos A.
    assert(schedule(SHELL_RSP) == PROCESS_A_RSP);

    // A se bloquea: el scheduler debe seleccionar B.
    processA.state = PROCESS_BLOCKED;
    assert(schedule(PROCESS_A_RSP) == PROCESS_B_RSP);
    assert(processA.state == PROCESS_BLOCKED);
    assert(getCurrentProcess() == &processB);

    // B termina: debe regresar a la shell.
    processB.state = PROCESS_TERMINATED;
    assert(schedule(PROCESS_B_RSP) == SHELL_RSP);
    assert(processB.state == PROCESS_TERMINATED);
    assert(getCurrentProcess() == NULL);

    // No hay procesos READY: la shell continúa.
    assert(schedule(SHELL_RSP) == SHELL_RSP);

    printf("OK: procesos bloqueados y terminados\n");
}

int main(void) {
    testSchedule();
    testBlockedAndTerminated();

    printf("OK: test_scheduler\n");
    return 0;
}