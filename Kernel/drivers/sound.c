#include <stdint.h>
#include <sound.h>
#include <timer.h>

/*
 * Frecuencia base del PIT.
 * El PC speaker usa el canal 2 del PIT para generar tonos.
 */
#define PIT_BASE_FREQUENCY 1193180

/*
 * Puertos usados para configurar el PIT y habilitar
 * el speaker interno de la PC.
 */
#define PIT_COMMAND_PORT   0x43
#define PIT_CHANNEL_2_PORT 0x42
#define SPEAKER_PORT       0x61

extern void outb(uint16_t port, uint8_t value);
extern uint8_t inb(uint16_t port);

/*
 * Estado del beep no bloqueante.
 *
 * beepActive indica si hay un sonido en curso.
 * beepEndTick indica en que tick debe apagarse.
 */
static volatile uint8_t beepActive = 0;
static volatile uint64_t beepEndTick = 0;

void soundStart(uint32_t frequency) {
    if (frequency == 0) {
        return;
    }

    uint32_t divisor = PIT_BASE_FREQUENCY / frequency;

    outb(PIT_COMMAND_PORT, 0xB6);
    outb(PIT_CHANNEL_2_PORT, divisor & 0xFF);
    outb(PIT_CHANNEL_2_PORT, (divisor >> 8) & 0xFF);

    uint8_t speakerState = inb(SPEAKER_PORT);
    outb(SPEAKER_PORT, speakerState | 0x03);
}

void soundStop(void) {
    uint8_t speakerState = inb(SPEAKER_PORT);
    outb(SPEAKER_PORT, speakerState & 0xFC);

    beepActive = 0;
}

void soundBeep(uint32_t frequency, uint64_t durationTicks) {
    if (frequency == 0 || durationTicks == 0) {
        return;
    }

    soundStart(frequency);

    beepEndTick = timerGetTicks() + durationTicks;
    beepActive = 1;
}

void soundTick(void) {
    if (beepActive && timerGetTicks() >= beepEndTick) {
        soundStop();
    }
}