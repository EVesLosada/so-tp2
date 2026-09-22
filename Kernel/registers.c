#include <stdint.h>
#include <registers.h>

/*
 * Ultimo snapshot de registros almacenado por el kernel.
 *
 * Se actualiza cuando el usuario presiona ESC y luego
 * puede recuperarse mediante la syscall SYS_GET_REGS.
 */
static RegisterFrame savedRegisters;

/*
 * Indica si ya se capturo al menos un snapshot valido.
 */
static uint8_t hasSnapshot = 0;

void saveRegistersSnapshot(RegisterFrame *frame) {
    uint64_t *dst = (uint64_t *) &savedRegisters;
    uint64_t *src = (uint64_t *) frame;

    for (uint64_t i = 0; i < sizeof(RegisterFrame) / sizeof(uint64_t); i++) {
        dst[i] = src[i];
    }

    hasSnapshot = 1;
}

uint8_t getRegistersSnapshot(RegisterFrame *destination) {
    if (destination == 0 || !hasSnapshot) {
        return 0;
    }

    uint64_t *dst = (uint64_t *) destination;
    uint64_t *src = (uint64_t *) &savedRegisters;

    for (uint64_t i = 0; i < sizeof(RegisterFrame) / sizeof(uint64_t); i++) {
        dst[i] = src[i];
    }

    return 1;
}