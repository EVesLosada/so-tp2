#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <keyboard.h>
#include <processManager.h>
#include <processTable.h>
#include <registers.h>

#define TEST_PROCESS_COUNT 4

static Process processes[TEST_PROCESS_COUNT];

Process *getProcessAt(uint64_t index) {
    if (index >= TEST_PROCESS_COUNT) {
        return 0;
    }

    return &processes[index];
}

Process *findProcessByPid(uint64_t pid) {
    for (uint64_t i = 0; i < TEST_PROCESS_COUNT; i++) {
        if (processes[i].pid == pid) {
            return &processes[i];
        }
    }

    return 0;
}

uint8_t unblockProcess(uint64_t pid) {
    (void)pid;
    return 0;
}

void saveRegistersSnapshot(RegisterFrame *frame) {
    (void)frame;
}

void terminalPutChar(char c) {
    (void)c;
}

int main(void) {
    memset(processes, 0, sizeof(processes));

    /* Shell raíz: debe sobrevivir a Ctrl+C. */
    processes[0].pid = 1;
    processes[0].parentPid = 0;
    processes[0].foreground = 1;
    processes[0].state = PROCESS_BLOCKED;
    processes[0].waitingForChildPid = 2;

    /* Hijo foreground: debe terminar. */
    processes[1].pid = 2;
    processes[1].parentPid = 1;
    processes[1].foreground = 1;
    processes[1].state = PROCESS_RUNNING;

    /* Hijo background: debe continuar. */
    processes[2].pid = 3;
    processes[2].parentPid = 1;
    processes[2].foreground = 0;
    processes[2].state = PROCESS_RUNNING;

    /* Proceso ya terminado: no debe modificarse. */
    processes[3].pid = 4;
    processes[3].parentPid = 1;
    processes[3].foreground = 1;
    processes[3].state = PROCESS_TERMINATED;

    RegisterFrame frame = {0};

    keyboardProcessScancode(0x1D, &frame);
    keyboardProcessScancode(0x2E, &frame);
    keyboardProcessScancode(0x9D, &frame);

    assert(processes[0].state == PROCESS_READY);
    assert(processes[1].state == PROCESS_TERMINATED);
    assert(processes[2].state == PROCESS_RUNNING);
    assert(processes[3].state == PROCESS_TERMINATED);

    puts("OK: Ctrl+C termina foreground sin terminar la shell");
    puts("OK: Ctrl+C conserva los procesos background");

    return 0;
}
