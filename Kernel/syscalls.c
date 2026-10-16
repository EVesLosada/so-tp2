#include <stdint.h>
#include <syscalls.h>
#include <terminal.h>
#include <keyboard.h>
#include <timer.h>
#include <videoDriver.h>
#include <sound.h>
#include <rtc.h>
#include <cracker.h>
#include <memoryManager.h>

#define STDIN  0
#define STDOUT 1

#define MIN_FONT_SCALE 1
#define MAX_FONT_SCALE 3

/*
 * Escribe caracteres en la terminal.
 *
 * Retorna la cantidad de caracteres escritos.
 */
static uint64_t sysWrite(uint64_t fd, const char *buffer, uint64_t count) {
    if (fd != STDOUT || buffer == 0) {
        return 0;
    }

    for (uint64_t i = 0; i < count; i++) {
        terminalPutChar(buffer[i]);
    }

    return count;
}

/*
 * Lee caracteres disponibles desde el buffer de teclado.
 *
 * Retorna la cantidad de caracteres leidos.
 */
static uint64_t sysRead(uint64_t fd, char *buffer, uint64_t count) {
    if (fd != STDIN || buffer == 0 || count == 0) {
        return 0;
    }

    uint64_t readCount = 0;

    while (readCount < count && keyboardReadChar(&buffer[readCount])) {
        readCount++;
    }

    return readCount;
}

/*
 * Limpia completamente la terminal y reinicia el cursor.
 */
static uint64_t sysClear(void) {
    terminalClear();
    return 0;
}

/*
 * Incrementa la escala de fuente de la terminal
 * hasta el maximo permitido.
 *
 * Retorna la nueva escala configurada.
 */
static uint64_t sysFontUp(void) {
    uint8_t scale = terminalGetScale();

    if (scale < MAX_FONT_SCALE) {
        terminalSetScale(scale + 1);
    }

    terminalClear();
    return terminalGetScale();
}

/*
 * Reduce la escala de fuente de la terminal
 * hasta el minimo permitido.
 *
 * Retorna la nueva escala configurada.
 */
static uint64_t sysFontDown(void) {
    uint8_t scale = terminalGetScale();

    if (scale > MIN_FONT_SCALE) {
        terminalSetScale(scale - 1);
    }

    terminalClear();
    return terminalGetScale();
}

/*
 * Copia el ultimo snapshot de registros almacenado
 * por el kernel al espacio de usuario.
 */
static uint64_t sysGetRegisters(RegisterFrame *destination) {
    return getRegistersSnapshot(destination);
}

/*
 * Devuelve la cantidad de ticks del timer.
 */
static uint64_t sysGetTicks(void) {
    return timerGetTicks();
}

/*
 * Dibuja un rectangulo en pantalla desde Userland.
 *
 * Valida que el rectangulo tenga dimensiones validas y que
 * no comience fuera de la pantalla. Si excede los limites,
 * recorta ancho o alto para evitar escribir fuera del framebuffer.
 *
 * Retorna:
 *   1 si dibujo el rectangulo
 *   0 si los parametros eran invalidos
 */
static uint64_t sysDrawRect(uint32_t color,
                            uint64_t x,
                            uint64_t y,
                            uint64_t width,
                            uint64_t height) {
    if (width == 0 || height == 0) {
        return 0;
    }

    if (x >= getScreenWidth() || y >= getScreenHeight()) {
        return 0;
    }

    if (x + width > getScreenWidth()) {
        width = getScreenWidth() - x;
    }

    if (y + height > getScreenHeight()) {
        height = getScreenHeight() - y;
    }

    drawRect(color, x, y, width, height);

    return 1;
}

/*
 * Inicia un beep no bloqueante mediante el driver de sonido.
 *
 * La duracion se mide en ticks y el apagado del speaker
 * se realiza desde el timer.
 */
static uint64_t sysBeep(uint32_t frequency, uint64_t durationTicks) {
    soundBeep(frequency, durationTicks);
    return 1;
}

/*
 * Devuelve la hora actual del RTC a Userland.
 *
 * Userland pasa tres punteros donde el kernel escribe:
 * - horas
 * - minutos
 * - segundos
 */
static uint64_t sysGetTime(uint8_t *hours, uint8_t *minutes, uint8_t *seconds) {
    if (hours == 0 || minutes == 0 || seconds == 0) {
        return 0;
    }

    rtcGetTime(hours, minutes, seconds);

    return 1;
}

/*
 * Guarda en kernel el hash objetivo que luego usara el Cracker.
 */
static uint64_t sysSetHash(uint64_t hash) {
    crackerSetHash(hash);
    return 1;
}

/*
 * Copia a Userland el hash objetivo actualmente cargado.
 *
 * Retorna:
 *   1 si habia un hash cargado
 *   0 si no habia hash o el puntero era invalido
 */
static uint64_t sysGetHash(uint64_t *hash) {
    return crackerGetHash(hash);
}

/*
 * Ejecuta el Kernel Cracker.
 *
 * Userland entrega:
 * - un buffer donde copiar la password encontrada
 * - el tamanio del buffer
 * - un puntero donde guardar los ciclos insumidos
 */
static uint64_t sysCrack(char *resultBuffer,
                         uint64_t resultBufferSize,
                         uint64_t *elapsedCycles) {
    return crackerCrack(resultBuffer, resultBufferSize, elapsedCycles);
}

uint64_t syscallDispatcher(RegisterFrame *frame) {
    switch (frame->rax) {
        case SYS_WRITE:
            return sysWrite(
                frame->rdi,
                (const char *) frame->rsi,
                frame->rdx
            );

        case SYS_READ:
            return sysRead(
                frame->rdi,
                (char *) frame->rsi,
                frame->rdx
            );

        case SYS_CLEAR:
            return sysClear();

        case SYS_FONT_UP:
            return sysFontUp();

        case SYS_FONT_DOWN:
            return sysFontDown();
        
        case SYS_GET_REGS:
            return sysGetRegisters((RegisterFrame *) frame->rdi);

        case SYS_GET_TICKS:
            return sysGetTicks();

        case SYS_DRAW_RECT:
            return sysDrawRect(
                (uint32_t) frame->rdi,
                frame->rsi,
                frame->rdx,
                frame->rcx,
                frame->r8
            );

        case SYS_BEEP:
            return sysBeep(
                (uint32_t) frame->rdi,
                frame->rsi
            );

        case SYS_GET_TIME:
            return sysGetTime(
                (uint8_t *) frame->rdi,
                (uint8_t *) frame->rsi,
                (uint8_t *) frame->rdx
            );

        case SYS_SET_HASH:
            return sysSetHash(frame->rdi);

        case SYS_GET_HASH:
            return sysGetHash((uint64_t *) frame->rdi);

        case SYS_CRACK:
            return sysCrack(
                (char *) frame->rdi,
                frame->rsi,
                (uint64_t *) frame->rdx
            );

        /*
         * Reservar memoria dinamica.
         *
         * RDI = cantidad de bytes solicitados.
         * RAX = direccion reservada o NULL.
         */
        case SYS_MALLOC:
            return (uint64_t)allocateMemory(frame->rdi);

        /*
         * Liberar un bloque previamente reservado.
         *
         * RDI = direccion del bloque.
         */
        case SYS_FREE:
            freeMemory((void *)(uintptr_t)frame->rdi);
            return 0;

        /*
         * Consultar el estado de la memoria.
         *
         * RDI = puntero a MemoryInfo.
         * RAX = 1 si se completo la consulta, 0 si es NULL.
         */
        case SYS_MEM_INFO:
            if (frame->rdi == 0) {
                return 0;
            }

            getMemoryInfo((MemoryInfo *)(uintptr_t)frame->rdi);
            return 1;

        default:
            return 0;
    }
}