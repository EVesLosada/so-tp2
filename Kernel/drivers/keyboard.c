#include <stdint.h>
#include <keyboard.h>
#include <registers.h>

#define KEYBOARD_BUFFER_SIZE 128

/*
 * Buffer circular de teclado.
 *
 * El handler de teclado escribe caracteres en writePosition
 * y Userland los consume mediante SYS_READ desde readPosition.
 */
static volatile char keyboardBuffer[KEYBOARD_BUFFER_SIZE];

/* Estado de ambas teclas Shift. */
static uint8_t leftShiftPressed = 0;
static uint8_t rightShiftPressed = 0;

static volatile uint64_t readPosition = 0;
static volatile uint64_t writePosition = 0;

/*
 * Tabla para scancode set 1.
 * El indice es el scancode; el valor es el caracter ASCII.
 *
 * Alcanza para:
 * - comandos de shell en minuscula
 * - Enter y Backspace
 * - teclas de Pacman: wasd / ijkl
 */
static const char keyMap[] = {
    0,    27,   '1',  '2',  '3',  '4',  '5',  '6',
    '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',
    'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',
    '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',
    'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',
    0,    ' '
};

#define KEY_MAP_SIZE (sizeof(keyMap) / sizeof(keyMap[0]))

/*
 * Traduccion de letras y numeros con Shift.
 *
 * Mapeo inicial latinoamericano para la fila numerica.
 * Los simbolos dependientes de AltGr se incorporaran
 * junto con el soporte de teclado avanzado.
 */
static char translateShift(uint8_t scancode, char base) {
    if (base >= 'a' && base <= 'z') {
        return base - 'a' + 'A';
    }

    switch (scancode) {
        case 0x02: return '!';
        case 0x03: return '"';
        case 0x04: return '#';
        case 0x05: return '$';
        case 0x06: return '%';
        case 0x07: return '&';
        case 0x08: return '/';
        case 0x09: return '(';
        case 0x0A: return ')';
        case 0x0B: return '=';
        default:   return base;
    }
}


/*
 * Agrega un caracter al buffer circular.
 *
 * Si el buffer esta lleno, descarta el caracter para evitar
 * sobrescribir datos pendientes de lectura.
 */
static void enqueueCharacter(char character) {
    uint64_t nextPosition = (writePosition + 1) % KEYBOARD_BUFFER_SIZE;

    if (nextPosition == readPosition) {
        return;
    }

    keyboardBuffer[writePosition] = character;
    writePosition = nextPosition;
}

void keyboardProcessScancode(uint8_t scancode, RegisterFrame *frame) {
    /*
     * ESC = scancode 0x01.
    */
    if (scancode == 0x01) {
        saveRegistersSnapshot(frame);
        return;
    }
    /*
     * En set 1, los codigos de liberacion tienen el bit alto en 1.
     * Solo almacenamos presiones de tecla.
     */
    if (scancode == 0x2A) {
        leftShiftPressed = 1;
        return;
    }

    if (scancode == 0x36) {
        rightShiftPressed = 1;
        return;
    }

    if (scancode == 0xAA) {
        leftShiftPressed = 0;
        return;
    }

    if (scancode == 0xB6) {
        rightShiftPressed = 0;
        return;
    }

    if (scancode & 0x80) {
        return;
    }

    if (scancode >= KEY_MAP_SIZE) {
        return;
    }

    char character = keyMap[scancode];

    if (leftShiftPressed || rightShiftPressed) {
        character = translateShift(scancode, character);
    }


    if (character != 0) {
        enqueueCharacter(character);
    }
}

uint8_t keyboardReadChar(char *character) {
    if (character == 0 || readPosition == writePosition) {
        return 0;
    }

    *character = keyboardBuffer[readPosition];
    readPosition = (readPosition + 1) % KEYBOARD_BUFFER_SIZE;

    return 1;
}