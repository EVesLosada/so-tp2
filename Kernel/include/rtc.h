#ifndef RTC_H
#define RTC_H

#include <stdint.h>

/*
 * Lee la hora actual desde el RTC/CMOS.
 *
 * Devuelve la hora en formato 24h:
 * - hours
 * - minutes
 * - seconds
 */
void rtcGetTime(uint8_t *hours, uint8_t *minutes, uint8_t *seconds);

#endif