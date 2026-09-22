#include <exceptions.h>
#include <terminal.h>

/*
 * Imprime un valor de 64 bits en hexadecimal.
 *
 * Se usa para mostrar registros y direcciones de memoria,
 * especialmente RIP, que indica donde ocurrio la excepcion.
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

    terminalPrint(buffer);
}

/*
 * Imprime un registro con formato:
 *
 * NOMBRE: 0x0000000000000000
 */
static void printRegister(const char *name, uint64_t value) {
    terminalPrint(name);
    terminalPrint(": ");
    printHex64(value);
    terminalPrint("\n");
}

/*
 * Imprime todos los registros guardados dentro del RegisterFrame.
 */
static void printRegisters(RegisterFrame *frame) {
    printRegister("RAX", frame->rax);
    printRegister("RBX", frame->rbx);
    printRegister("RCX", frame->rcx);
    printRegister("RDX", frame->rdx);
    printRegister("RBP", frame->rbp);
    printRegister("RDI", frame->rdi);
    printRegister("RSI", frame->rsi);
    printRegister("RSP", frame->rsp);
    printRegister("R8 ", frame->r8);
    printRegister("R9 ", frame->r9);
    printRegister("R10", frame->r10);
    printRegister("R11", frame->r11);
    printRegister("R12", frame->r12);
    printRegister("R13", frame->r13);
    printRegister("R14", frame->r14);
    printRegister("R15", frame->r15);
    printRegister("RIP", frame->rip);
    printRegister("CS ", frame->cs);
    printRegister("RFL", frame->rflags);
}

void exceptionDispatcher(uint64_t exception, RegisterFrame *frame) {
    /*
     * Limpiamos la terminal para que cada excepcion se vea completa.
     * Si no, al imprimir muchos registros, las primeras lineas pueden
     * desplazarse fuera de la pantalla.
     */
    terminalClear();
    terminalPrint("========================================\n");

    switch (exception) {
        case DIVIDE_BY_ZERO_EXCEPTION:
            terminalPrint("EXCEPCION: DIVISION POR CERO\n");
            break;

        case INVALID_OPCODE_EXCEPTION:
            terminalPrint("EXCEPCION: INVALID OPCODE\n");
            break;

        default:
            terminalPrint("EXCEPCION DESCONOCIDA\n");
            break;
    }

    terminalPrint("\n");
    printRegisters(frame);

    terminalPrint("========================================\n");
}