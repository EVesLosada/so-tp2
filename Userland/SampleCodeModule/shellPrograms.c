#include <shellPrograms.h>
#include <syscalls.h>

/*
 * Programa echo.
 *
 * Imprime los argumentos recibidos y finaliza.
 * Puede ejecutarse en foreground o background.
 */
void shellEchoProcess(uint64_t argc, char **argv) {
    for (uint64_t i = 1; i < argc; i++) {
        if (i > 1) {
            sysWrite(STDOUT, " ", 1);
        }

        uint64_t length = 0;

        while (argv[i][length] != '\0') {
            length++;
        }

        sysWrite(STDOUT, argv[i], length);
    }

    sysWrite(STDOUT, "\n", 1);
}

/*
 * Programa cat.
 *
 * Copia STDIN a STDOUT hasta recibir EOF.
 * Sirve como consumidor de datos en un pipeline.
 */
void shellCatProcess(uint64_t argc, char **argv) {
    (void)argc;
    (void)argv;

    char buffer[128];
    uint64_t bytesRead;

    while ((bytesRead = sysRead(STDIN, buffer, sizeof(buffer))) > 0) {
        uint64_t written = 0;

        while (written < bytesRead) {
            uint64_t count = sysWrite(
                STDOUT,
                buffer + written,
                bytesRead - written
            );

            if (count == 0) {
                return;
            }

            written += count;
        }
    }
}

/*
 * Funciones auxiliares para imprimir desde programas de Userland.
 */
static void shellProgramPrint(const char *text) {
    uint64_t length = 0;

    while (text[length] != '\0') {
        length++;
    }

    sysWrite(STDOUT, text, length);
}

static void shellProgramPrintDecimal(uint64_t value) {
    char digits[20];
    uint64_t count = 0;

    do {
        digits[count++] = '0' + (value % 10);
        value /= 10;
    } while (value != 0);

    while (count > 0) {
        count--;
        sysWrite(STDOUT, &digits[count], 1);
    }
}

/*
 * Programa mem.
 * Muestra memoria total, usada y libre en bytes.
 */
void shellMemProcess(uint64_t argc, char **argv) {
    (void)argc;
    (void)argv;

    MemoryInfo info;

    if (!sysMemInfo(&info)) {
        shellProgramPrint("ERROR: no se pudo consultar la memoria\n");
        return;
    }

    shellProgramPrint("Memoria total: ");
    shellProgramPrintDecimal(info.totalMemory);
    shellProgramPrint(" bytes\n");

    shellProgramPrint("Memoria usada: ");
    shellProgramPrintDecimal(info.usedMemory);
    shellProgramPrint(" bytes\n");

    shellProgramPrint("Memoria libre: ");
    shellProgramPrintDecimal(info.freeMemory);
    shellProgramPrint(" bytes\n");
}

/*
 * Snapshot estatico para no ocupar el stack del proceso.
 */
#define SHELL_PS_CAPACITY 64

static ProcessInfo shellPsSnapshot[SHELL_PS_CAPACITY];

static void shellProgramPrintHex(uint64_t value) {
    const char hex[] = "0123456789ABCDEF";
    char buffer[18];

    buffer[0] = '0';
    buffer[1] = 'x';

    for (uint64_t i = 0; i < 16; i++) {
        buffer[2 + i] = hex[(value >> (60 - i * 4)) & 0xF];
    }

    sysWrite(STDOUT, buffer, sizeof(buffer));
}

static const char *shellProcessStateName(ProcessState state) {
    switch (state) {
        case PROCESS_READY:
            return "READY";
        case PROCESS_RUNNING:
            return "RUNNING";
        case PROCESS_BLOCKED:
            return "BLOCKED";
        case PROCESS_TERMINATED:
            return "TERMINATED";
        default:
            return "UNKNOWN";
    }
}

/*
 * Programa ps.
 * Muestra la informacion de los procesos del sistema.
 */
void shellPsProcess(uint64_t argc, char **argv) {
    (void)argc;
    (void)argv;

    uint64_t count = sysPs(shellPsSnapshot, SHELL_PS_CAPACITY);

    if (count > SHELL_PS_CAPACITY) {
        count = SHELL_PS_CAPACITY;
    }

    shellProgramPrint("Procesos: ");
    shellProgramPrintDecimal(count);
    shellProgramPrint("\n");

    for (uint64_t i = 0; i < count; i++) {
        ProcessInfo *process = &shellPsSnapshot[i];

        shellProgramPrint("PID: ");
        shellProgramPrintDecimal(process->pid);

        shellProgramPrint(" | Nombre: ");
        shellProgramPrint(process->name);

        shellProgramPrint(" | Estado: ");
        shellProgramPrint(
            shellProcessStateName(process->state)
        );

        shellProgramPrint(" | Prioridad: ");
        shellProgramPrintDecimal(process->priority);

        shellProgramPrint(" | ");
        shellProgramPrint(
            process->foreground ? "FG" : "BG"
        );
        shellProgramPrint("\n");

        shellProgramPrint("  RSP: ");
        shellProgramPrintHex(process->stackPointer);

        shellProgramPrint(" | RBP: ");
        shellProgramPrintHex(process->basePointer);
        shellProgramPrint("\n");
    }
}

/*
 * Programa loop.
 *
 * Permanece ejecutandose e imprime su PID
 * cada cierta cantidad de ticks del sistema.
 *
 * sysYield permite que otros procesos utilicen la CPU.
 */
#define SHELL_LOOP_INTERVAL_TICKS 100ULL

void shellLoopProcess(uint64_t argc, char **argv) {
    (void)argv;

    if (argc != 1) {
        shellProgramPrint("Uso: loop\n");
        return;
    }

    uint64_t pid = sysGetPid();
    uint64_t lastPrintTick = sysGetTicks();

    shellProgramPrint("loop iniciado - PID ");
    shellProgramPrintDecimal(pid);
    shellProgramPrint("\n");

    for (;;) {
        uint64_t currentTick = sysGetTicks();

        if (currentTick - lastPrintTick >=
            SHELL_LOOP_INTERVAL_TICKS) {

            shellProgramPrint("loop ejecutando - PID ");
            shellProgramPrintDecimal(pid);
            shellProgramPrint("\n");

            lastPrintTick = currentTick;
        }

        sysYield();
    }
}
