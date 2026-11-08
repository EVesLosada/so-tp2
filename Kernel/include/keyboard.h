#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <registers.h>

/*
 * Procesa un scancode recibido desde IRQ1.
 * Si corresponde a una tecla soportada, la guarda utilizando el RegisterFrame recibido.
 */
void keyboardProcessScancode(uint8_t scancode, RegisterFrame *frame);

/*
 * Intenta leer un caracter pendiente.
 * Devuelve 1 si habia un caracter y 0 si el buffer estaba vacio.
 */
/* 0 = vacio, 1 = caracter, 2 = EOF (Ctrl+D). */
uint8_t keyboardReadChar(char *character);
void keyboardSetWaitingProcess(uint64_t pid);

#endif