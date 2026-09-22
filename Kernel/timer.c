#include <stdint.h>
#include <timer.h>
#include <sound.h>

/*
 * Contador global de ticks.
 */
static volatile uint64_t ticks = 0;

void timerTick(void) {
    ticks++;

    /*
     * Permite apagar beeps no bloqueantes cuando se cumple
     * su duracion. Esto evita esperar dentro de una syscall.
     */
    soundTick();
}

uint64_t timerGetTicks(void) {
    return ticks;
}