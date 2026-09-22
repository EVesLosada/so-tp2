#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

/*
 * Configura el PIT canal 2 y enciende el PC speaker
 * para emitir un tono con la frecuencia indicada.
 */
void soundStart(uint32_t frequency);

/*
 * Apaga el PC speaker y marca el beep como finalizado.
 */
void soundStop(void);

/*
 * Inicia un beep no bloqueante.
 *
 * La funcion vuelve inmediatamente. El timer se encarga
 * de apagar el sonido cuando se alcanza beepEndTick.
 */
void soundBeep(uint32_t frequency, uint64_t durationTicks);

// Actualiza el estado del beep.
void soundTick(void);

#endif