
#include <processTable.h>
#include <stddef.h>

/*
 * Tabla estatica de punteros a PCB.
 *
 * No reserva memoria dinamicamente:
 * cada PCB sera creado por otro componente
 * y la tabla solamente guardara su direccion.
 */
static Process *processTable[MAX_PROCESSES];

static uint64_t processCount = 0;

void initializeProcessTable(void) {
    for (uint64_t i = 0; i < MAX_PROCESSES; i++) {
        processTable[i] = NULL;
    }

    processCount = 0;
}

uint8_t registerProcess(Process *process) {
    if (process == NULL || findProcessByPid(process->pid) != NULL) {
        return 0;
    }

    for (uint64_t i = 0; i < MAX_PROCESSES; i++) {
        if (processTable[i] == NULL) {
            processTable[i] = process;
            processCount++;
            return 1;
        }
    }

    return 0;
}

Process *findProcessByPid(uint64_t pid) {
    for (uint64_t i = 0; i < MAX_PROCESSES; i++) {
        if (processTable[i] != NULL &&
            processTable[i]->pid == pid) {
            return processTable[i];
        }
    }

    return NULL;
}

uint8_t unregisterProcess(uint64_t pid) {
    for (uint64_t i = 0; i < MAX_PROCESSES; i++) {
        if (processTable[i] != NULL &&
            processTable[i]->pid == pid) {
            processTable[i] = NULL;
            processCount--;
            return 1;
        }
    }

    return 0;
}

uint64_t getProcessCount(void) {
    return processCount;
}
