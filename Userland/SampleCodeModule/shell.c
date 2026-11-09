#include <stdint.h>
#include <syscalls.h>
#include <shellParser.h>
#include <shellPrograms.h>
#include <pacman.h>

#define COMMAND_BUFFER_SIZE 64

#define SHELL_BACKGROUND_CAPACITY 16
#define SHELL_SNAPSHOT_CAPACITY 64

static uint64_t backgroundPids[SHELL_BACKGROUND_CAPACITY];
static uint64_t backgroundCount = 0;

/* Seguimiento de pipelines ejecutados en background. */
#define SHELL_BACKGROUND_PIPELINE_CAPACITY 8

typedef struct {
    uint64_t pipeId;
    uint64_t writerPid;
    uint64_t readerPid;
} ShellBackgroundPipeline;

static ShellBackgroundPipeline backgroundPipelines[
    SHELL_BACKGROUND_PIPELINE_CAPACITY
];

static uint64_t backgroundPipelineCount = 0;

/* Identificador para nombres de pipes temporales. */
static uint64_t nextShellPipeId = 1;

/*
 * Genera nombres shpipe1, shpipe2, etc.
 * El buffer debe tener espacio para al menos 27 caracteres.
 */
static void makeShellPipeName(char name[32]) {
    const char prefix[] = "shpipe";
    uint64_t index = 0;

    for (uint64_t i = 0; prefix[i] != '\0'; i++) {
        name[index++] = prefix[i];
    }

    uint64_t value = nextShellPipeId++;
    char digits[20];
    uint64_t count = 0;

    do {
        digits[count++] = '0' + (value % 10);
        value /= 10;
    } while (value != 0);

    while (count > 0) {
        name[index++] = digits[--count];
    }

    name[index] = '\0';
}

static ProcessInfo backgroundSnapshot[SHELL_SNAPSHOT_CAPACITY];

#define TSC_FREQUENCY_HZ 2500000000ULL

static uint64_t stringLength(const char *str);
static void clearScreen(void);
static void print(const char *str);
static void printHex64(uint64_t value);
static void printRegister(const char *name, uint64_t value);
static uint8_t stringsEqual(const char *first, const char *second);
static void printPrompt(void);
static void executeCommand(const char *command);
static void triggerDivZero(void);
static void triggerInvalidOpcode(void);
static void printDecimal(uint64_t value);

/*
 * Calcula la longitud de una cadena terminada en cero.
 */
static uint64_t stringLength(const char *str) {
    uint64_t length = 0;

    while (str[length] != 0) {
        length++;
    }

    return length;
}

/*
 * Limpia la terminal usando la syscall SYS_CLEAR.
 *
 * Se usa antes de imprimir informacion extensa,
 * como registros o excepciones, para evitar
 * que el contenido quede mezclado en pantalla.
 */
static void clearScreen(void) {
    sysClear();
}

/*
 * Imprime una cadena en STDOUT mediante SYS_WRITE.
 */
static void print(const char *str) {
    sysWrite(STDOUT, str, stringLength(str));
}

/*
 * Imprime un valor de 64 bits en formato hexadecimal.
 *
 * Se utiliza principalmente para mostrar registros.
 */
static void printHex64(uint64_t value) {
    char buffer[18];
    char hexDigits[] = "0123456789ABCDEF";

    buffer[0] = '0';
    buffer[1] = 'x';

    for (uint64_t i = 0; i < 16; i++) {
        uint64_t shift = 60 - i * 4;
        buffer[2 + i] = hexDigits[(value >> shift) & 0xF];
    }

    sysWrite(STDOUT, buffer, 18);
}

/*
 * Imprime un registro con formato:
 *
 * NOMBRE: 0x0000000000000000
 */
static void printRegister(const char *name, uint64_t value) {
    print(name);
    print(": ");
    printHex64(value);
    print("\n");
}

/*
 * Compara dos cadenas terminadas en cero.
 *
 * Retorna 1 si son iguales o 0 en caso contrario.
 */
static uint8_t stringsEqual(const char *first, const char *second) {
    uint64_t i = 0;

    while (first[i] != 0 && second[i] != 0) {
        if (first[i] != second[i]) {
            return 0;
        }

        i++;
    }

    return first[i] == second[i];
}

/*
 * Verifica si una cadena comienza con el prefijo indicado.
 */
static uint8_t startsWith(const char *str, const char *prefix) {
    uint64_t i = 0;

    while (prefix[i] != 0) {
        if (str[i] != prefix[i]) {
            return 0;
        }

        i++;
    }

    return 1;
}

/*
 * Avanza el puntero hasta el primer caracter que no sea espacio.
 */
static const char *skipSpaces(const char *str) {
    while (*str == ' ') {
        str++;
    }

    return str;
}

/*
 * Convierte una cadena decimal sin signo a uint64_t.
 *
 * Retorna 1 si la conversion fue exitosa o 0 si el
 * formato era invalido.
 */
static uint8_t parseUnsignedDecimal(const char *str, uint64_t *value) {
    uint64_t result = 0;
    uint8_t hasDigits = 0;

    if (value == 0) {
        return 0;
    }

    str = skipSpaces(str);

    while (*str >= '0' && *str <= '9') {
        uint64_t digit = (uint64_t)(*str - '0');

        if (result > (UINT64_MAX - digit) / 10) {
            return 0;
        }

        result = result * 10 + digit;
        str++;
        hasDigits = 1;
    }

    str = skipSpaces(str);

    if (*str != 0) {
        return 0;
    }

    if (!hasDigits) {
        return 0;
    }

    *value = result;
    return 1;
}

/*
 * Imprime el prompt de la shell.
 */
static void printPrompt(void) {
    print("> ");
}

/*
 * Imprime un valor numerico usando siempre dos digitos.
 *
 * Ejemplo:
 *  3  -> 03
 * 12  -> 12
 */
static void printTwoDigits(uint8_t value) {
    char digits[2];

    digits[0] = '0' + (value / 10);
    digits[1] = '0' + (value % 10);

    sysWrite(STDOUT, digits, 2);
}

/*
 * Imprime un entero sin signo en base 10.
 */
static void printDecimal(uint64_t value) {
    char buffer[21];
    uint64_t index = 0;

    if (value == 0) {
        print("0");
        return;
    }

    while (value > 0 && index < sizeof(buffer)) {
        buffer[index++] = '0' + (value % 10);
        value /= 10;
    }

    while (index > 0) {
        index--;
        sysWrite(STDOUT, &buffer[index], 1);
    }
}

/*
 * Imprime un tiempo aproximado en segundos a partir de ciclos TSC.
 *
 * La conversion usa una frecuencia estimada del TSC.
 * Sirve para mostrar el tiempo insumido como pide el enunciado.
 */
static void printApproxSeconds(uint64_t cycles) {
    uint64_t seconds = cycles / TSC_FREQUENCY_HZ;
    uint64_t milliseconds = ((cycles % TSC_FREQUENCY_HZ) * 1000) / TSC_FREQUENCY_HZ;

    printDecimal(seconds);
    print(".");

    if (milliseconds < 100) {
        print("0");
    }

    if (milliseconds < 10) {
        print("0");
    }

    printDecimal(milliseconds);
    print(" s");
}

/*
 * Imprime la lista de comandos disponibles.
 *
 * Se reutiliza desde help, fontup y fontdown para que,
 * al cambiar el tamaño de fuente, la shell vuelva a mostrar
 * la lista de comandos en el nuevo tamaño.
 */
/*
 * Recoge hijos en background que ya finalizaron.
 *
 * Nunca espera a un hijo que todavia esta ejecutandose.
 */
/*
 * Marca un proceso de pipeline como finalizado.
 * Un PID igual a cero indica que ya fue liberado.
 */
static void shellMarkPipelineChildFinished(uint64_t pid) {
    for (uint64_t i = 0; i < backgroundPipelineCount; i++) {
        ShellBackgroundPipeline *pipeline = &backgroundPipelines[i];

        if (pipeline->writerPid == pid) {
            pipeline->writerPid = 0;
        }

        if (pipeline->readerPid == pid) {
            pipeline->readerPid = 0;
        }
    }
}

/*
 * Libera los pipes cuyos dos procesos ya finalizaron.
 */
static void shellCleanupBackgroundPipelines(void) {
    uint64_t i = 0;

    while (i < backgroundPipelineCount) {
        ShellBackgroundPipeline *pipeline = &backgroundPipelines[i];

        if (pipeline->writerPid != 0 || pipeline->readerPid != 0) {
            i++;
            continue;
        }

        if (!sysPipeDestroy(pipeline->pipeId)) {
            print("ERROR: no se pudo liberar el pipe de background\n");
            i++;
            continue;
        }

        backgroundPipelineCount--;
        backgroundPipelines[i] =
            backgroundPipelines[backgroundPipelineCount];
    }
}

static void shellReapBackgroundChildren(void) {
    if (backgroundCount == 0) {
        return;
    }

    uint64_t count = sysPs(
        backgroundSnapshot,
        SHELL_SNAPSHOT_CAPACITY
    );

    uint64_t i = 0;

    while (i < backgroundCount) {
        uint64_t pid = backgroundPids[i];
        uint8_t found = 0;
        uint8_t reaped = 0;

        for (uint64_t j = 0; j < count; j++) {
            if (backgroundSnapshot[j].pid != pid) {
                continue;
            }

            found = 1;

            if (backgroundSnapshot[j].state ==
                PROCESS_TERMINATED) {
                reaped = (sysWait(pid) == pid);
            }

            break;
        }

        if (reaped) {
            print("[bg] finalizado PID ");
            printDecimal(pid);
            print("\n");
        } else if (!found) {
            print("[bg] PID ya no registrado: ");
            printDecimal(pid);
            print("\n");
        }

        if (reaped || !found) {
            /*
             * Si el PID ya no aparece en sysPs, el kernel
             * ya lo retiro de la tabla de procesos.
             * En ambos casos el hijo dejo de existir.
             */
            shellMarkPipelineChildFinished(pid);

            backgroundCount--;
            backgroundPids[i] =
                backgroundPids[backgroundCount];
            continue;
        }

        i++;
    }

    shellCleanupBackgroundPipelines();
}

static void printHelp(void) {
    print("Comandos:\n");
    print("  help      - ayuda\n");
    print("  clear     - limpiar pantalla\n");
    print("  fontup    - agrandar texto\n");
    print("  fontdown  - achicar texto\n");
    print("  regs      - mostrar registros\n");
    print("  ticks     - mostrar ticks\n");
    print("  divzero   - probar division por cero\n");
    print("  invalidop - probar invalid opcode\n");
    print("  drawdemo  - demo grafica\n");
    print("  beep      - sonido\n");
    print("  pacman    - jugar Pacman\n");
    print("  time      - hora del sistema\n");
    print("  sethash N - guardar hash\n");
    print("  gethash   - mostrar hash\n");
    print("  crack     - buscar password\n");
    print("  echo      - imprimir argumentos\n");
    print("  cat       - copiar STDIN a STDOUT\n");
    print("  wc        - contar lineas de STDIN\n");
    print("  filter    - eliminar vocales de STDIN\n");
    print("  mem       - mostrar estado de memoria\n");
    print("  test_mm N - probar memoria (maximo N bytes)\n");
    print("  ps        - listar procesos\n");
    print("  loop      - proceso repetitivo\n");
    print("  kill PID  - terminar proceso\n");
    print("  nice P N  - cambiar prioridad (1-5)\n");
    print("  block PID - bloquear proceso\n");
    print("  unblock P - desbloquear proceso\n");
    print("  jobs      - procesos en background\n");

    print("\nPipelines y control:\n");
    print("  comando1 | comando2 - conectar STDOUT con STDIN\n");
    print("  comando &            - ejecutar en background\n");
    print("  Ctrl+C               - terminar procesos foreground\n");
    print("  Ctrl+D               - enviar EOF a quien lee STDIN\n");
    print("  Ctrl+P               - escribir el operador |\n");
    print("  F6                   - escribir el operador | (alternativa)\n");
    print("  Shift+-              - escribir el caracter _\n");
}

/*
 * Dispatcher de comandos de la shell.
 *
 * Recibe la línea ingresada por el usuario y ejecuta
 * la funcionalidad correspondiente utilizando syscalls
 * hacia el kernel cuando es necesario.
 */
typedef void (*ShellProgramEntry)(uint64_t argc, char **argv);

/*
 * Registro de programas ejecutables.
 * Se ampliara al incorporar los comandos del TP2.
 */
static ShellProgramEntry findShellProgram(const char *name) {
    if (stringsEqual(name, "echo")) {
        return shellEchoProcess;
    }

    if (stringsEqual(name, "filter")) {
        return shellFilterProcess;
    }

    if (stringsEqual(name, "wc")) {
        return shellWcProcess;
    }

    if (stringsEqual(name, "cat")) {
        return shellCatProcess;
    }

    if (stringsEqual(name, "mem")) {
        return shellMemProcess;
    }

    if (stringsEqual(name, "ps")) {
        return shellPsProcess;
    }

    if (stringsEqual(name, "loop")) {
        return shellLoopProcess;
    }

    if (stringsEqual(name, "test_mm")) {
        return shellTestMmProcess;
    }

    if (stringsEqual(name, "test_proc")) {
        return shellTestProcProcess;
    }

    if (stringsEqual(name, "test_prio")) {
        return shellTestPrioProcess;
    }

    if (stringsEqual(name, "test_sync")) {
        return shellTestSyncProcess;
    }

    return 0;
}

/*
 * Ejecuta dos programas conectados por un pipe.
 *
 * El primer proceso escribe en el pipe.
 * El segundo lee del pipe.
 */
static void executePipeline(const ShellParsedCommand *parsed) {
    if (parsed->background &&
        (backgroundCount > SHELL_BACKGROUND_CAPACITY - 2 ||
         backgroundPipelineCount >=
             SHELL_BACKGROUND_PIPELINE_CAPACITY)) {
        print("ERROR: demasiados procesos en background\n");
        return;
    }

    ShellProgramEntry left = findShellProgram(parsed->argv[0]);
    ShellProgramEntry right = findShellProgram(parsed->pipeArgv[0]);

    if (left == 0 || right == 0) {
        print("ERROR: programa desconocido en pipeline\n");
        return;
    }

    char pipeName[32];
    makeShellPipeName(pipeName);

    uint64_t pipeId = sysPipeCreate(pipeName);

    if (pipeId == 0) {
        print("ERROR: no se pudo crear el pipe\n");
        return;
    }

    uint64_t readFd = sysPipeOpen(pipeName, PIPE_READ);
    uint64_t writeFd = sysPipeOpen(pipeName, PIPE_WRITE);

    if (readFd >= 16 || writeFd >= 16) {
        print("ERROR: no se pudieron abrir los extremos\n");

        if (readFd < 16) {
            sysClose(readFd);
        }

        if (writeFd < 16) {
            sysClose(writeFd);
        }

        sysPipeDestroy(pipeId);
        return;
    }

    ProcessCreateIOArgs writerArgs = {
        .name = parsed->argv[0],
        .entryPoint = left,
        .priority = 1,
        .foreground = parsed->background ? 0 : 1,
        .argc = parsed->argc,
        .argv = (const char *const *)parsed->argv,
        .stdinFd = STDIN,
        .stdoutFd = writeFd
    };

    ProcessCreateIOArgs readerArgs = {
        .name = parsed->pipeArgv[0],
        .entryPoint = right,
        .priority = 1,
        .foreground = parsed->background ? 0 : 1,
        .argc = parsed->pipeArgc,
        .argv = (const char *const *)parsed->pipeArgv,
        .stdinFd = readFd,
        .stdoutFd = STDOUT
    };

    /*
     * Crear ambos hijos antes de esperar.
     * El lector debe existir para consumir los datos.
     */
    uint64_t writerPid =
        sysCreateProcessWithRedirectedIO(&writerArgs);

    uint64_t readerPid = 0;

    if (writerPid != 0) {
        readerPid = sysCreateProcessWithRedirectedIO(&readerArgs);
    }

    /*
     * Si falla la creacion del lector, terminar el escritor
     * antes de cerrar los extremos originales.
     */
    if (writerPid != 0 && readerPid == 0) {
        sysKill(writerPid);
    }

    /*
     * Los hijos ya heredaron sus descriptores.
     * La shell debe liberar los suyos para permitir EOF.
     */
    sysClose(writeFd);
    sysClose(readFd);

    if (parsed->background &&
        writerPid != 0 && readerPid != 0) {

        backgroundPipelines[backgroundPipelineCount++] =
            (ShellBackgroundPipeline){
                .pipeId = pipeId,
                .writerPid = writerPid,
                .readerPid = readerPid
            };

        backgroundPids[backgroundCount++] = writerPid;
        backgroundPids[backgroundCount++] = readerPid;

        print("[bg] pipeline iniciado PID ");
        printDecimal(writerPid);
        print(" y PID ");
        printDecimal(readerPid);
        print("\n");
        return;
    }

    if (writerPid != 0) {
        while (sysWait(writerPid) == 0) {
            sysYield();
        }
    }

    if (readerPid != 0) {
        while (sysWait(readerPid) == 0) {
            sysYield();
        }
    }

    if (!sysPipeDestroy(pipeId)) {
        print("ERROR: no se pudo liberar el pipe\n");
    }

    if (writerPid == 0 || readerPid == 0) {
        print("ERROR: no se pudo crear el pipeline\n");
    }
}

static void executeCommand(const char *command) {
    shellReapBackgroundChildren();
    ShellParsedCommand parsed;

    if (!shellParseCommand(command, &parsed)) {
        print("Error: sintaxis de comando invalida\n");
        return;
    }

    if (parsed.argc == 0) {
        return;
    }

    /*
     * Si hay un segundo comando, ejecutar ambos
     * conectados por un pipe.
     */
    if (parsed.pipeArgc > 0) {
        executePipeline(&parsed);
        return;
    }

    ShellProgramEntry program =
        findShellProgram(parsed.argv[0]);

    if (program != 0) {
        if (parsed.background &&
            backgroundCount >= SHELL_BACKGROUND_CAPACITY) {
            print("ERROR: demasiados procesos en background\n");
            return;
        }

        uint64_t pid = sysCreateProcess(
            parsed.argv[0],
            program,
            1,
            parsed.background ? 0 : 1,
            parsed.argc,
            (const char *const *)parsed.argv
        );

        if (pid == 0) {
            print("ERROR: no se pudo crear el proceso\n");
            return;
        }

        if (parsed.background) {
            backgroundPids[backgroundCount++] = pid;

            print("[bg] iniciado PID ");
            printDecimal(pid);
            print("\n");
            return;
        }

        print("Shell: esperando al hijo PID ");
        printDecimal(pid);
        print("\n");

        uint64_t finished;
        while ((finished = sysWait(pid)) == 0) {
            sysYield();
        }

        if (finished == pid) {
            print("Shell: hijo finalizado correctamente\n");
        } else {
            print("ERROR: sysWait fallo\n");
        }

        return;
    }

    if (parsed.background) {
        print("Ese comando no admite background\n");
        return;
    }

    if (stringsEqual(parsed.argv[0], "block")) {
        uint64_t pid = 0;

        if (parsed.argc != 2 ||
            !parseUnsignedDecimal(parsed.argv[1], &pid) ||
            pid == 0) {
            print("Uso: block PID\n");
            return;
        }

        /* Evitar bloquear la shell. */
        if (pid == 1 || pid == sysGetPid()) {
            print("ERROR: no se puede bloquear la shell\n");
            return;
        }

        if (!sysBlock(pid)) {
            print("ERROR: no se pudo bloquear el proceso\n");
            return;
        }

        print("Proceso bloqueado: PID ");
        printDecimal(pid);
        print("\n");
        return;
    }

    if (stringsEqual(parsed.argv[0], "unblock")) {
        uint64_t pid = 0;

        if (parsed.argc != 2 ||
            !parseUnsignedDecimal(parsed.argv[1], &pid) ||
            pid == 0) {
            print("Uso: unblock PID\n");
            return;
        }

        if (!sysUnblock(pid)) {
            print("ERROR: no se pudo desbloquear el proceso\n");
            return;
        }

        print("Proceso desbloqueado: PID ");
        printDecimal(pid);
        print("\n");
        return;
    }

    if (stringsEqual(parsed.argv[0], "nice")) {
        uint64_t pid = 0;
        uint64_t priority = 0;

        if (parsed.argc != 3 ||
            !parseUnsignedDecimal(parsed.argv[1], &pid) ||
            !parseUnsignedDecimal(parsed.argv[2], &priority) ||
            pid == 0 ||
            priority < 1 ||
            priority > 5) {
            print("Uso: nice PID PRIORIDAD (1-5)\n");
            return;
        }

        if (!sysNice(pid, priority)) {
            print("ERROR: no se pudo cambiar la prioridad\n");
            return;
        }

        print("Prioridad actualizada - PID ");
        printDecimal(pid);
        print(" - Prioridad ");
        printDecimal(priority);
        print("\n");
        return;
    }

    if (stringsEqual(parsed.argv[0], "kill")) {
        uint64_t pid = 0;

        if (parsed.argc != 2 ||
            !parseUnsignedDecimal(parsed.argv[1], &pid) ||
            pid == 0) {
            print("Uso: kill PID\n");
            return;
        }

        /* Evitar terminar la propia shell. */
        if (pid == 1 || pid == sysGetPid()) {
            print("ERROR: no se puede terminar la shell\n");
            return;
        }

        if (!sysKill(pid)) {
            print("ERROR: no se pudo terminar el proceso\n");
            return;
        }

        print("Proceso terminado: PID ");
        printDecimal(pid);
        print("\n");

        shellReapBackgroundChildren();
        return;
    }

    if (stringsEqual(parsed.argv[0], "jobs")) {
        print("[bg] pendientes: ");
        printDecimal(backgroundCount);
        print("\n");

        for (uint64_t i = 0; i < backgroundCount; i++) {
            print("[bg] PID ");
            printDecimal(backgroundPids[i]);
            print("\n");
        }

        return;
    }

    /*
     * Los comandos internos existentes conservan
     * su comportamiento anterior.
     */
    command = skipSpaces(command);

    if (stringsEqual(command, "help")) {
        printHelp();
        return;
    }

    if (stringsEqual(command, "clear")) {
        sysClear();
        return;
    }

    if (stringsEqual(command, "fontup")) {
        sysFontUp();
        printHelp();
        return;
    }

    if (stringsEqual(command, "fontdown")) {
        sysFontDown();
        printHelp();
        return;
    }

    if (stringsEqual(command, "regs")) {

        clearScreen();

        RegisterFrame registers;

        if (!sysGetRegisters(&registers)) {
            print("No hay snapshot de registros. Presiona ESC primero.\n");
            return;
        }

        printRegister("RAX", registers.rax);
        printRegister("RBX", registers.rbx);
        printRegister("RCX", registers.rcx);
        printRegister("RDX", registers.rdx);
        printRegister("RBP", registers.rbp);
        printRegister("RDI", registers.rdi);
        printRegister("RSI", registers.rsi);
        printRegister("RSP", registers.rsp);
        printRegister("R8 ", registers.r8);
        printRegister("R9 ", registers.r9);
        printRegister("R10", registers.r10);
        printRegister("R11", registers.r11);
        printRegister("R12", registers.r12);
        printRegister("R13", registers.r13);
        printRegister("R14", registers.r14);
        printRegister("R15", registers.r15);
        printRegister("RIP", registers.rip);
        printRegister("CS ", registers.cs);
        printRegister("RFL", registers.rflags);

        return;
    }

    if (stringsEqual(command, "divzero")) {
        triggerDivZero();
        return;
    }

    if (stringsEqual(command, "invalidop")) {
        triggerInvalidOpcode();
        return;
    }

    /*
    * Muestra la cantidad de ticks transcurridos desde
    * el inicio del sistema.
    */
    if (stringsEqual(command, "ticks")) {
        print("Ticks: ");
        printDecimal(sysGetTicks());
        print("\n");
        return;
    }

    /*
    * Comando de diagnostico grafico.
    *
    * Verifica que Userland pueda solicitar al kernel
    * el dibujo de rectangulos mediante SYS_DRAW_RECT.
    * Esta syscall es utilizada posteriormente por Pacman.
    */
    if (stringsEqual(command, "drawdemo")) {
        sysClear();

        print("Draw test ejecutado\n");
        print("Usa clear para limpiar la pantalla\n");

        sysDrawRect(0x000000FF, 80, 100, 120, 80);
        sysDrawRect(0x0000FF00, 240, 100, 160, 60);
        sysDrawRect(0x00FF0000, 160, 240, 220, 90);

        return;
    }

    if (stringsEqual(command, "beep")) {
        print("Beep\n");
        sysBeep(440, 6);
        return;
    }

    if (stringsEqual(command, "pacman")) {
        runPacman();
        return;
    }

    if (stringsEqual(command, "time")) {
        uint8_t hours;
        uint8_t minutes;
        uint8_t seconds;

        if (!sysGetTime(&hours, &minutes, &seconds)) {
            print("No se pudo leer la hora\n");
            return;
        }

        printTwoDigits(hours);
        print(":");
        printTwoDigits(minutes);
        print(":");
        printTwoDigits(seconds);
        print("\n");

        return;
    }

    if (startsWith(command, "sethash")) {
        uint64_t hash;

        if (command[7] != ' ') {
            print("Uso: sethash <numero>\n");
            return;
        }

        if (!parseUnsignedDecimal(command + 7, &hash)) {
            print("Uso: sethash <numero>\n");
            return;
        }

        sysSetHash(hash);
        print("Hash guardado\n");
        return;
    }

    if (stringsEqual(command, "gethash")) {
        uint64_t hash;

        if (!sysGetHash(&hash)) {
            print("No hay hash cargado\n");
            return;
        }

        print("Hash actual: ");
        printDecimal(hash);
        print("\n");
        return;
    }

    /*
    * Ejecuta el Kernel Cracker.
    *
    * El kernel intenta encontrar una password de 5 letras minusculas
    * cuyo hash coincida con el hash objetivo guardado.
    */
    if (stringsEqual(command, "crack")) {
        char result[6];
        uint64_t elapsedCycles = 0;

        if (!sysCrack(result, sizeof(result), &elapsedCycles)) {
            print("No se encontro password\n");
            print("Ciclos: ");
            printDecimal(elapsedCycles);
            print("\n");
            print("Tiempo aprox: ");
            printApproxSeconds(elapsedCycles);
            print("\n");
            return;
        }

        print("Password encontrada: ");
        print(result);
        print("\n");

        print("Ciclos: ");
        printDecimal(elapsedCycles);
        print("\n");

        print("Tiempo aprox: ");
        printApproxSeconds(elapsedCycles);
        print("\n");

        return;
    }

    if (command[0] != 0) {
        print("Comando desconocido\n");
    }
}

/*
 * Fuerza una division por cero real a nivel CPU.
 *
 * Usamos assembler para evitar que el compilador optimice
 * o modifique la division en C, ya que dividir por cero en C
 * es comportamiento indefinido.
 */
static void triggerDivZero(void) {
    __asm__ volatile (
        "mov $1, %%rax\n"
        "xor %%rdx, %%rdx\n"
        "xor %%rcx, %%rcx\n"
        "div %%rcx\n"
        :
        :
        : "rax", "rdx", "rcx"
    );
}

/*
 * Fuerza una instruccion invalida.
 *
 * ud2 es una instruccion definida por x86 para generar
 * explicitamente la excepcion invalid opcode.
 */
static void triggerInvalidOpcode(void) {
    __asm__ volatile ("ud2");
}

int main() {
    char command[COMMAND_BUFFER_SIZE];
    uint64_t commandLength = 0;
    char character;

    print("TPE ARQUI 2026\n");
    print("Escriba help para ver los comandos disponibles.\n");
    print("\n");
    printPrompt();

    while (1) {
        if (sysRead(STDIN, &character, 1) == 0) {
            continue;
        }

        if (character == '\n') {
            sysWrite(STDOUT, &character, 1);

            command[commandLength] = 0;
            executeCommand(command);

            commandLength = 0;
            printPrompt();
            continue;
        }

        if (character == '\b') {
            if (commandLength > 0) {
                commandLength--;
                sysWrite(STDOUT, &character, 1);
            }

            continue;
        }

        if (commandLength < COMMAND_BUFFER_SIZE - 1) {
            command[commandLength] = character;
            commandLength++;

            sysWrite(STDOUT, &character, 1);
        }
    }

    return 0;
}