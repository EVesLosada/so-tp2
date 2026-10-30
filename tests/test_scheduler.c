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

static void testPriorityScheduling(void) {
    initializeProcessManager();

    Process processA = {0};
    Process processB = {0};

    processA.pid = 1;
    processA.state = PROCESS_READY;
    processA.priority = 1;
    processA.savedStackPointer = PROCESS_A_RSP;

    processB.pid = 2;
    processB.state = PROCESS_READY;
    processB.priority = 3;
    processB.savedStackPointer = PROCESS_B_RSP;

    assert(registerProcess(&processA));
    assert(registerProcess(&processB));

    uint64_t executionsA = 0;
    uint64_t executionsB = 0;

    /*
     * Simulamos selecciones desde la shell.
     * Cada proceso vuelve a READY antes de la
     * siguiente seleccion.
     */
    for (uint64_t i = 0; i < 40; i++) {
        Process *selected = selectNextReadyProcess();

        assert(selected != NULL);

        if (selected == &processA) {
            executionsA++;
        } else if (selected == &processB) {
            executionsB++;
        } else {
            assert(0);
        }
    }

    printf(
        "Prioridades: A=%llu, B=%llu\n",
        (unsigned long long)executionsA,
        (unsigned long long)executionsB
    );

    /*
     * B tiene prioridad 3 y A prioridad 1.
     * Esperamos que B reciba mas oportunidades.
     */
    assert(executionsB > executionsA);

    printf("OK: scheduler respeta prioridades\n");
}

static void testReusedProcessSlot(void) {
    initializeProcessManager();

    Process processA = {0};
    Process processB = {0};
    Process processC = {0};

    processA.pid = 1;
    processA.state = PROCESS_READY;
    processA.priority = 5;
    processA.savedStackPointer = PROCESS_A_RSP;

    processB.pid = 2;
    processB.state = PROCESS_READY;
    processB.priority = 1;
    processB.savedStackPointer = PROCESS_B_RSP;

    assert(registerProcess(&processA));
    assert(registerProcess(&processB));

    /* A ejecuta un turno y conserva cuatro creditos. */
    assert(selectNextReadyProcess() == &processA);

    /* A termina y se libera su posicion. */
    processA.state = PROCESS_TERMINATED;
    assert(unregisterProcess(processA.pid));

    /* C ocupa la posicion que tenia A. */
    processC.pid = 3;
    processC.state = PROCESS_READY;
    processC.priority = 1;
    processC.savedStackPointer = 0x4000ULL;

    assert(registerProcess(&processC));
    assert(getProcessAt(0) == &processC);

    uint64_t executionsB = 0;
    uint64_t executionsC = 0;

    for (uint64_t i = 0; i < 20; i++) {
        Process *selected = selectNextReadyProcess();

        assert(selected != NULL);

        if (selected == &processB) {
            executionsB++;
        } else if (selected == &processC) {
            executionsC++;
        } else {
            assert(0);
        }
    }

    printf(
        "Reutilizacion: B=%llu, C=%llu\n",
        (unsigned long long)executionsB,
        (unsigned long long)executionsC
    );

    /*
     * B y C tienen la misma prioridad.
     * No deben conservarse los creditos de A.
     */
    assert(executionsB == executionsC);

    printf("OK: reutilizacion de slot sin creditos heredados\n");
}

static void testPriorityChange(void) {
    initializeProcessManager();

    Process processA = {0};
    Process processB = {0};

    processA.pid = 1;
    processA.state = PROCESS_READY;
    processA.priority = 1;
    processA.savedStackPointer = PROCESS_A_RSP;

    processB.pid = 2;
    processB.state = PROCESS_READY;
    processB.priority = 1;
    processB.savedStackPointer = PROCESS_B_RSP;

    assert(registerProcess(&processA));
    assert(registerProcess(&processB));

    uint64_t countA = 0;
    uint64_t countB = 0;

    /* Con igual prioridad, deben alternarse. */
    for (uint64_t i = 0; i < 20; i++) {
        Process *selected = selectNextReadyProcess();

        if (selected == &processA) countA++;
        if (selected == &processB) countB++;
    }

    assert(countA == 10);
    assert(countB == 10);

    /* Cambiar la prioridad de B mientras existe. */
    assert(setProcessPriority(processB.pid, 3));

    countA = 0;
    countB = 0;

    for (uint64_t i = 0; i < 40; i++) {
        Process *selected = selectNextReadyProcess();

        if (selected == &processA) countA++;
        if (selected == &processB) countB++;
    }

    printf(
        "Despues de nice: A=%llu, B=%llu\n",
        (unsigned long long)countA,
        (unsigned long long)countB
    );

    assert(countB > countA);

    printf("OK: nice modifica la planificacion\n");
}

int main(void) {
    testSchedule();
    testBlockedAndTerminated();
    testPriorityScheduling();
    testReusedProcessSlot();
    testPriorityChange();

    printf("OK: test_scheduler\n");
    return 0;
}