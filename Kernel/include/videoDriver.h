#ifndef VIDEODRIVER_H
#define VIDEODRIVER_H

#include <stdint.h>

void putPixel(uint32_t hexColor, uint64_t x, uint64_t y);
void drawRect(uint32_t hexColor, uint64_t x, uint64_t y, uint64_t width, uint64_t height);
void clearScreen(uint32_t hexColor);
void drawChar(char c, uint64_t x, uint64_t y, uint32_t fgColor, uint32_t bgColor, uint8_t scale);
void drawString(const char *str, uint64_t x, uint64_t y, uint32_t fgColor, uint32_t bgColor, uint8_t scale);

uint16_t getScreenWidth();
uint16_t getScreenHeight();

#endif