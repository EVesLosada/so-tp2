#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>

/*
 * Inicializa la terminal grafica.
 */
void terminalInit();
/*
 * Limpia la pantalla y reinicia el cursor.
 */
void terminalClear();

/*
 * Imprime un caracter en la posicion actual del cursor.
 *
 * Maneja salto de linea, backspace y avance automatico
 * al llegar al borde derecho de la pantalla.
 */
void terminalPutChar(char c);
/*
 * Imprime una cadena terminada en cero.
 */
void terminalPrint(const char *str);

/*
 * Cambia la escala de la fuente.
 *
 * La validacion de maximo/minimo se realiza desde la syscall,
 * aca solo se evita aceptar escala cero.
 */
void terminalSetScale(uint8_t scale);
/*
 * Devuelve la escala actual de la fuente.
 */
uint8_t terminalGetScale();

/*
 * Cambia los colores usados por la terminal.
 */
void terminalSetColor(uint32_t fgColor, uint32_t bgColor);

#endif