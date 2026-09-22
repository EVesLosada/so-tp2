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
    if (scancode & 0x80) {
        return;
    }

    if (scancode >= KEY_MAP_SIZE) {
        return;
    }

    char character = keyMap[scancode];

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