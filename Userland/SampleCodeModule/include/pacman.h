#ifndef PACMAN_H
#define PACMAN_H

/*
 * Ejecuta una partida completa de Pacman.
 *
 * El juego utiliza:
 * - SYS_READ para leer teclado.
 * - SYS_DRAW_RECT para dibujar el tablero.
 * - SYS_GET_TICKS para temporizacion.
 * - SYS_BEEP para efectos de sonido.
 *
 * La funcion retorna cuando el jugador gana,
 * pierde o decide salir con la tecla q.
 */
void runPacman(void);

#endif