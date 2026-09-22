#include <stdint.h>
#include <rtc.h>

#define RTC_ADDRESS_PORT 0x70
#define RTC_DATA_PORT    0x71

#define RTC_SECONDS_REGISTER  0x00
#define RTC_MINUTES_REGISTER  0x02
#define RTC_HOURS_REGISTER    0x04
#define RTC_STATUS_A_REGISTER 0x0A
#define RTC_STATUS_B_REGISTER 0x0B

extern void outb(uint16_t port, uint8_t value);
extern uint8_t inb(uint16_t port);

/*
 * Lee un registro del RTC/CMOS.
 */
static uint8_t rtcReadRegister(uint8_t reg) {
    outb(RTC_ADDRESS_PORT, reg);
    return inb(RTC_DATA_PORT);
}

/*
 * Indica si el RTC esta actualizando sus valores.
 *
 * Si esta actualizando, conviene esperar para no leer
 * una hora mezclada entre dos segundos distintos.
 */
static uint8_t rtcIsUpdating(void) {
    return rtcReadRegister(RTC_STATUS_A_REGISTER) & 0x80;
}

/*
 * Convierte BCD a binario normal.
 *
 * Algunos RTC entregan los valores como BCD:
 * por ejemplo 0x25 representa 25.
 */
static uint8_t bcdToBinary(uint8_t value) {
    return (value & 0x0F) + ((value >> 4) * 10);
}

void rtcGetTime(uint8_t *hours, uint8_t *minutes, uint8_t *seconds) {
    uint8_t firstHours;
    uint8_t firstMinutes;
    uint8_t firstSeconds;

    uint8_t currentHours;
    uint8_t currentMinutes;
    uint8_t currentSeconds;

    uint8_t statusB;
    uint8_t pm;

    do {
        while (rtcIsUpdating()) {
        }

        firstSeconds = rtcReadRegister(RTC_SECONDS_REGISTER);
        firstMinutes = rtcReadRegister(RTC_MINUTES_REGISTER);
        firstHours = rtcReadRegister(RTC_HOURS_REGISTER);

        while (rtcIsUpdating()) {
        }

        currentSeconds = rtcReadRegister(RTC_SECONDS_REGISTER);
        currentMinutes = rtcReadRegister(RTC_MINUTES_REGISTER);
        currentHours = rtcReadRegister(RTC_HOURS_REGISTER);

    } while (
        firstSeconds != currentSeconds ||
        firstMinutes != currentMinutes ||
        firstHours != currentHours
    );

    /*
    * El registro B indica si los valores estan en BCD/binario
    * y si el reloj usa formato de 12 o 24 horas.
    */
    statusB = rtcReadRegister(RTC_STATUS_B_REGISTER);

    pm = currentHours & 0x80;
    currentHours &= 0x7F;

    if ((statusB & 0x04) == 0) {
        currentSeconds = bcdToBinary(currentSeconds);
        currentMinutes = bcdToBinary(currentMinutes);
        currentHours = bcdToBinary(currentHours);
    }

    if ((statusB & 0x02) == 0) {
        if (pm && currentHours < 12) {
            currentHours += 12;
        }

        if (!pm && currentHours == 12) {
            currentHours = 0;
        }
    }

    *hours = currentHours;
    *minutes = currentMinutes;
    *seconds = currentSeconds;
}