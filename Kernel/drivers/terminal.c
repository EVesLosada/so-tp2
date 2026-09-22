#include <terminal.h>
#include <videoDriver.h>

/*
 * Margenes iniciales de la terminal grafica.
 */
#define TERMINAL_MARGIN_X 20
#define TERMINAL_MARGIN_Y 20

/*
 * Dimensiones base de cada caracter de la fuente bitmap.
 * El tamaño final depende de fontScale.
 */
#define FONT_WIDTH 8
#define FONT_HEIGHT 15

/*
 * Posicion actual del cursor dentro de la terminal.
 */
static uint64_t cursorX = TERMINAL_MARGIN_X;
static uint64_t cursorY = TERMINAL_MARGIN_Y;

static uint32_t foregroundColor = 0x00FFFFFF;
static uint32_t backgroundColor = 0x00000000;

/*
 * Escala actual de la fuente.
 * Arranca en 1 para permitir fontup desde el inicio.
 */
static uint8_t fontScale = 1;

/*
 * Devuelve el ancho real de un caracter considerando la escala actual.
 */
static uint64_t charWidth() {
    return FONT_WIDTH * fontScale;
}

/*
 * Devuelve el alto real de un caracter considerando la escala actual.
 */
static uint64_t charHeight() {
    return FONT_HEIGHT * fontScale;
}

/*
 * Avanza el cursor a la siguiente linea.
 *
 * Si se llega al final de la pantalla, limpia la terminal.
 */
static void terminalNewline() {
    cursorX = TERMINAL_MARGIN_X;
    cursorY += charHeight();

    if (cursorY + charHeight() >= getScreenHeight()) {
        terminalClear();
    }
}

void terminalInit() {
    terminalClear();
}

void terminalClear() {
    clearScreen(backgroundColor);
    cursorX = TERMINAL_MARGIN_X;
    cursorY = TERMINAL_MARGIN_Y;
}

void terminalPutChar(char c) {
    if (c == '\n') {
        terminalNewline();
        return;
    }

    if (c == '\b') {
        if (cursorX > TERMINAL_MARGIN_X) {
            cursorX -= charWidth();
            drawChar(' ', cursorX, cursorY, foregroundColor, backgroundColor, fontScale);
        }
        return;
    }

    drawChar(c, cursorX, cursorY, foregroundColor, backgroundColor, fontScale);
    cursorX += charWidth();

    if (cursorX + charWidth() >= getScreenWidth()) {
        terminalNewline();
    }
}

void terminalPrint(const char *str) {
    for (uint64_t i = 0; str[i] != 0; i++) {
        terminalPutChar(str[i]);
    }
}

void terminalSetScale(uint8_t scale) {
    if (scale == 0) {
        return;
    }

    fontScale = scale;
}

uint8_t terminalGetScale() {
    return fontScale;
}

void terminalSetColor(uint32_t fgColor, uint32_t bgColor) {
    foregroundColor = fgColor;
    backgroundColor = bgColor;
}