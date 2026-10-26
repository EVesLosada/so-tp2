#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <process.h>
#include <processTable.h>
#include <processManager.h>
#include <memoryManager.h>
#include <registers.h>

/*
 * Simulacion del Memory Manager para las pruebas.
 * processManager.c y processTable.c son los reales.
 */
static uint64_t allocationCalls = 0;
static uint64_t activeAllocations = 0;
static uint64_t failOnCall = 0;

void *allocateMemory(uint64_t size) {
    allocationCalls++;

    if (failOnCall == allocationCalls) {
        return NULL;
    }

    void *ptr = malloc((size_t)size);

    if (ptr != NULL) {
        activeAllocations++;
    }

    return ptr;
}

void freeMemory(void *ptr) {
    if (ptr != NULL) {
        assert(activeAllocations > 0);
        activeAllocations--;
        free(ptr);
    }
}

static void dummyEntry(void) {
}

static void checkProcessContext(Process *process) {
    assert(process != NULL);
    assert(process->state == PROCESS_READY);
    assert(process->stackBase != NULL);
    assert(process->stackSize == PROCESS_STACK_SIZE);

    uintptr_t start = (uintptr_t)process->stackBase;
    uintptr_t end = start + process->stackSize;
    uintptr_t saved = process->savedStackPointer;

    assert(saved >= start);
    assert(saved + sizeof(RegisterFrame) <= end);

    RegisterFrame *frame = (RegisterFrame *)saved;

    assert(frame->rip != 0);
    assert(frame->cs == 0x08);
    assert(frame->rflags == 0x202);
    assert(frame->rdi == (uint64_t)(uintptr_t)process);
    assert(frame->rsp == saved + sizeof(RegisterFrame));
    assert((frame->rsp & 15) == 8);
}

int main(void) {
    initializeProcessManager();

    /* Tabla inicialmente vacia. */
    assert(getProcessCount() == 0);

    /* Rechazar parametros invalidos. */
    assert(createProcess(NULL, dummyEntry, 1, 0, 1) == 0);
    assert(createProcess("", dummyEntry, 1, 0, 1) == 0);
    assert(createProcess("test", NULL, 1, 0, 1) == 0);
    assert(getProcessCount() == 0);

    /* Fallo al reservar PCB. */
    failOnCall = allocationCalls + 1;

    assert(createProcess("test", dummyEntry, 1, 0, 1) == 0);
    assert(getProcessCount() == 0);
    assert(activeAllocations == 0);

    /* Fallo al reservar stack: debe liberar el PCB. */
    failOnCall = allocationCalls + 2;

    assert(createProcess("test", dummyEntry, 1, 0, 1) == 0);
    assert(getProcessCount() == 0);
    assert(activeAllocations == 0);

    failOnCall = 0;

    /* Crear procesos hasta llenar la tabla. */
    uint64_t pids[MAX_PROCESSES];

    for (uint64_t i = 0; i < MAX_PROCESSES; i++) {
        pids[i] = createProcess(
            "proceso",
            dummyEntry,
            2,
            0,
            1
        );

        assert(pids[i] == i + 1);

        Process *process = findProcessByPid(pids[i]);

        assert(process != NULL);
        assert(process->pid == pids[i]);
        assert(process->priority == 2);
        assert(process->parentPid == 0);
        assert(process->foreground == 1);
        assert(strcmp(process->name, "proceso") == 0);
        assert(process->entryPoint == dummyEntry);

        checkProcessContext(process);
    }

    assert(getProcessCount() == MAX_PROCESSES);
    assert(activeAllocations == MAX_PROCESSES * 2);

    /* Comprobar que los primeros stacks son independientes. */
    Process *first = findProcessByPid(pids[0]);
    Process *second = findProcessByPid(pids[1]);

    assert(first->stackBase != second->stackBase);
    assert(first->savedStackPointer != second->savedStackPointer);

    /* No debe aceptar un proceso adicional. */
    assert(createProcess("extra", dummyEntry, 1, 0, 1) == 0);
    assert(getProcessCount() == MAX_PROCESSES);

        /*
    * Pruebas de bloqueo y desbloqueo de procesos.
    */

    /* No se puede operar sobre un PID inexistente. */
    assert(blockProcess(0) == 0);
    assert(unblockProcess(0) == 0);

    /* Bloquear un proceso READY. */
    assert(blockProcess(pids[0]) == 1);
    assert(first->state == PROCESS_BLOCKED);

    /* No se puede bloquear dos veces. */
    assert(blockProcess(pids[0]) == 0);
    assert(first->state == PROCESS_BLOCKED);

    /* El scheduler no debe seleccionar el proceso bloqueado. */
    Process *selected = selectNextReadyProcess();

    assert(selected != NULL);
    assert(selected != first);
    assert(selected->state == PROCESS_READY);

    /* Desbloquear debe devolverlo al estado READY. */
    assert(unblockProcess(pids[0]) == 1);
    assert(first->state == PROCESS_READY);

    /* No se puede desbloquear dos veces. */
    assert(unblockProcess(pids[0]) == 0);

    /* No se puede bloquear ni desbloquear un proceso terminado. */
    first->state = PROCESS_TERMINATED;

    assert(blockProcess(pids[0]) == 0);
    assert(unblockProcess(pids[0]) == 0);

    first->state = PROCESS_READY;

    /* No se puede bloquear un proceso en estado RUNNING
    * con esta primera version de la interfaz.
    */
    first->state = PROCESS_RUNNING;

    assert(blockProcess(pids[0]) == 0);
    assert(unblockProcess(pids[0]) == 0);

    first->state = PROCESS_READY;

    /* Las operaciones no deben modificar la tabla. */
    assert(getProcessCount() == MAX_PROCESSES);

    puts("OK: pruebas de bloqueo y desbloqueo superadas");

        /*
    * Pruebas de terminacion de procesos.
    */

    /* No se puede terminar un PID inexistente. */
    assert(terminateProcess(0) == 0);

    /* Terminar un proceso READY. */
    assert(terminateProcess(pids[0]) == 1);
    assert(first->state == PROCESS_TERMINATED);

    /* No se puede terminar dos veces. */
    assert(terminateProcess(pids[0]) == 0);

    /* Un proceso terminado no puede bloquearse ni desbloquearse. */
    assert(blockProcess(pids[0]) == 0);
    assert(unblockProcess(pids[0]) == 0);

    /* Tambien se puede terminar un proceso BLOCKED. */
    assert(blockProcess(pids[1]) == 1);
    assert(second->state == PROCESS_BLOCKED);

    assert(terminateProcess(pids[1]) == 1);
    assert(second->state == PROCESS_TERMINATED);

    /* No se puede terminar directamente un proceso RUNNING. */
    Process *third = findProcessByPid(pids[2]);

    assert(third != NULL);
    third->state = PROCESS_RUNNING;

    assert(terminateProcess(pids[2]) == 0);
    assert(third->state == PROCESS_RUNNING);

    third->state = PROCESS_READY;

    /* Terminar un proceso no libera recursos todavia. */
    assert(getProcessCount() == MAX_PROCESSES);
    assert(activeAllocations == MAX_PROCESSES * 2);

    puts("OK: pruebas de terminacion superadas");

    /*
     * Limpieza desde el test.
     * La destruccion definitiva corresponde al Bloque 10.
     */
    for (uint64_t i = 0; i < MAX_PROCESSES; i++) {
        Process *process = findProcessByPid(pids[i]);

        assert(process != NULL);
        assert(unregisterProcess(pids[i]) == 1);

        freeMemory(process->stackBase);
        freeMemory(process);
    }

    assert(getProcessCount() == 0);
    assert(activeAllocations == 0);

        /*
    * Pruebas de liberacion de procesos terminados.
    */
    uint64_t pidReady = createProcess(
        "terminado_ready", dummyEntry, 1, 0, 0
    );

    uint64_t pidBlocked = createProcess(
        "terminado_blocked", dummyEntry, 1, 0, 0
    );

    uint64_t pidAlive = createProcess(
        "proceso_vivo", dummyEntry, 1, 0, 0
    );

    assert(pidReady != 0);
    assert(pidBlocked != 0);
    assert(pidAlive != 0);

    assert(getProcessCount() == 3);
    assert(activeAllocations == 6);

    /* Terminar un proceso READY. */
    assert(terminateProcess(pidReady) == 1);

    /* Terminar otro que estaba BLOCKED. */
    assert(blockProcess(pidBlocked) == 1);
    assert(terminateProcess(pidBlocked) == 1);

    /* Debe liberar solamente los dos terminados. */
    assert(reapTerminatedProcesses() == 2);

    assert(findProcessByPid(pidReady) == NULL);
    assert(findProcessByPid(pidBlocked) == NULL);

    assert(getProcessCount() == 1);
    assert(activeAllocations == 2);

    /* El proceso vivo debe conservarse intacto. */
    Process *alive = findProcessByPid(pidAlive);

    assert(alive != NULL);
    assert(alive->state == PROCESS_READY);

    /* No debe liberar nada si no hay terminados. */
    assert(reapTerminatedProcesses() == 0);

    /* Finalmente liberar tambien el ultimo proceso. */
    assert(terminateProcess(pidAlive) == 1);
    assert(reapTerminatedProcesses() == 1);

    assert(getProcessCount() == 0);
    assert(activeAllocations == 0);

    puts("OK: liberacion de procesos terminados superada");

        /*
    * Prueba de limpieza segura despues de un cambio
    * de contexto del scheduler.
    */
    uint64_t pidRunning = createProcess(
        "proceso_running", dummyEntry, 1, 0, 0
    );

    assert(pidRunning != 0);
    assert(getProcessCount() == 1);
    assert(activeAllocations == 2);

    /*
    * Simular una interrupcion de la shell:
    * el scheduler selecciona nuestro proceso.
    *
    * Los punteros de contexto usados en este test
    * son simulados; no se restauran registros reales.
    */
    uint64_t shellRsp = 0x12345000ULL;
    uint64_t processRsp = schedule(shellRsp);

    Process *running = getCurrentProcess();

    assert(running != NULL);
    assert(running->pid == pidRunning);
    assert(running->state == PROCESS_RUNNING);
    assert(processRsp == running->savedStackPointer);

    /*
    * Simular que processBootstrap marca el proceso
    * como terminado, pero aun sigue siendo el actual.
    */
    running->state = PROCESS_TERMINATED;

    /* Todavia NO se debe liberar su memoria. */
    assert(reapTerminatedProcesses() == 0);
    assert(getProcessCount() == 1);
    assert(activeAllocations == 2);

    /*
    * Simular la siguiente interrupcion:
    * el scheduler abandona el proceso terminado
    * y vuelve a la shell.
    */
    assert(schedule(processRsp) == shellRsp);
    assert(getCurrentProcess() == NULL);

    /* Ahora si es seguro liberar sus recursos. */
    assert(reapTerminatedProcesses() == 1);
    assert(findProcessByPid(pidRunning) == NULL);
    assert(getProcessCount() == 0);
    assert(activeAllocations == 0);

    puts("OK: limpieza segura despues del scheduler superada");

    /*
    * Prueba de bloqueo del proceso actualmente ejecutandose.
    *
    * En Linux se simula la interrupcion de cambio de contexto
    * llamando directamente a schedule().
    */
    uint64_t pidSelf = createProcess(
        "autobloqueo", dummyEntry, 1, 0, 0
    );

    assert(pidSelf != 0);

    Process *self = findProcessByPid(pidSelf);
    assert(self != NULL);
    assert(self->state == PROCESS_READY);

    /* Simular que el scheduler empieza a ejecutar este proceso. */
    uint64_t shellRsp2 = 0x22220000ULL;
    uint64_t selfRsp = schedule(shellRsp2);

    assert(getCurrentProcess() == self);
    assert(self->state == PROCESS_RUNNING);
    assert(selfRsp == self->savedStackPointer);

    /* El proceso se bloquea a si mismo. */
    assert(blockProcess(pidSelf) == 1);
    assert(self->state == PROCESS_BLOCKED);

    /*
    * En el test de Linux, INT 0x81 no se ejecuta.
    * Simulamos el cambio de contexto por separado.
    */
    assert(schedule(selfRsp) == shellRsp2);
    assert(getCurrentProcess() == NULL);

    /* La shell desbloquea el proceso. */
    assert(unblockProcess(pidSelf) == 1);
    assert(self->state == PROCESS_READY);

    /* El scheduler puede volver a ejecutarlo. */
    assert(schedule(shellRsp2) == selfRsp);
    assert(getCurrentProcess() == self);
    assert(self->state == PROCESS_RUNNING);

    /* Simular su finalizacion. */
    self->state = PROCESS_TERMINATED;

    assert(schedule(selfRsp) == shellRsp2);
    assert(getCurrentProcess() == NULL);

    /* Liberar sus recursos. */
    assert(reapTerminatedProcesses() == 1);
    assert(findProcessByPid(pidSelf) == NULL);
    assert(getProcessCount() == 0);
    assert(activeAllocations == 0);

    puts("OK: autobloqueo y reanudacion simulados");

    puts("OK: pruebas del administrador de procesos superadas");
    return 0;
}
