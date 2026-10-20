#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <processTable.h>
#include <processManager.h>

static void testRoundRobin(void) {
    initializeProcessManager();

    Process first = {0};
    first.pid = 1;
    first.state = PROCESS_READY;

    Process second = {0};
    second.pid = 2;
    second.state = PROCESS_READY;

    Process third = {0};
    third.pid = 3;
    third.state = PROCESS_READY;

    assert(registerProcess(&first) == 1);
    assert(registerProcess(&second) == 1);
    assert(registerProcess(&third) == 1);

    assert(selectNextReadyProcess() == &first);
    assert(selectNextReadyProcess() == &second);
    assert(selectNextReadyProcess() == &third);

    /* Al llegar al final, vuelve al primero. */
    assert(selectNextReadyProcess() == &first);

    /* Un proceso que no esta READY no debe seleccionarse. */
    second.state = PROCESS_TERMINATED;

    assert(selectNextReadyProcess() == &third);
    assert(selectNextReadyProcess() == &first);

    /* Si ninguno esta READY, devuelve NULL. */
    first.state = PROCESS_TERMINATED;
    third.state = PROCESS_TERMINATED;

    assert(selectNextReadyProcess() == NULL);

    printf("OK: test_round_robin\n");
}

int main(void) {
    initializeProcessTable();

    assert(getProcessCount() == 0);
    assert(getProcessAt(0) == NULL);
    assert(getProcessAt(MAX_PROCESSES) == NULL);

    Process first = {0};
    first.pid = 1;

    Process second = {0};
    second.pid = 2;

    assert(registerProcess(&first) == 1);
    assert(registerProcess(&second) == 1);

    assert(getProcessCount() == 2);
    assert(getProcessAt(0) == &first);
    assert(getProcessAt(1) == &second);

    assert(unregisterProcess(1) == 1);

    assert(getProcessAt(0) == NULL);
    assert(getProcessAt(1) == &second);
    assert(getProcessCount() == 1);

    assert(getProcessAt(MAX_PROCESSES) == NULL);

    printf("OK: test_process_table\n");

    testRoundRobin();
    
    return 0;
}