#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/*
 * Incrementa el contador global de ticks.
 */
void timerTick(void);

/*
 * Devuelve la cantidad de ticks transcurridos desde que
 * el kernel tomo control del timer.
 */
uint64_t timerGetTicks(void);

#endif