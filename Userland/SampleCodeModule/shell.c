#include <stdint.h>
#include <syscalls.h>
#include <pacman.h>

#define COMMAND_BUFFER_SIZE 64
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
        result = result * 10 + (*str - '0');
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
}

/*
 * Dispatcher de comandos de la shell.
 *
 * Recibe la línea ingresada por el usuario y ejecuta
 * la funcionalidad correspondiente utilizando syscalls
 * hacia el kernel cuando es necesario.
 */
static void executeCommand(const char *command) {
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