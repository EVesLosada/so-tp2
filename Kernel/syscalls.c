#include <semaphore.h>
#include <stdint.h>
#include <stddef.h>
#include <syscalls.h>
#include <terminal.h>
#include <keyboard.h>
#include <timer.h>
#include <videoDriver.h>
#include <sound.h>
#include <rtc.h>
#include <cracker.h>
#include <memoryManager.h>
#include <processManager.h>

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

    /*
     * La shell todavia no tiene PCB.
     *
     * Cuando atiende una syscall, aprovechamos para
     * recuperar los recursos de procesos terminados.
     *
     * No hacemos esta limpieza desde el timer:
     * liberar memoria durante una interrupcion de
     * planificacion podria ser peligroso.
     */
    if (getCurrentProcess() == NULL) {
        reapTerminatedProcesses();
    }

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

        /*
         * Crear un proceso con argumentos.
         *
         * RDI = nombre
         * RSI = punto de entrada
         * RDX = prioridad
         * RCX = foreground (0 o 1)
         * R8  = argc
         * R9  = argv
         *
         * El PID del padre lo determina el Kernel.
         * RAX = PID creado o 0 si falla.
         */
        case SYS_CREATE_PROC: {
            if (frame->rcx > 1) {
                return 0;
            }

            Process *parent = getCurrentProcess();
            uint64_t parentPid = parent != NULL ? parent->pid : 0;

            return createProcessWithArgs(
                (const char *)(uintptr_t)frame->rdi,
                (void (*)(uint64_t, char **))(uintptr_t)frame->rsi,
                frame->rdx,
                parentPid,
                (uint8_t)frame->rcx,
                frame->r8,
                (const char *const *)(uintptr_t)frame->r9
            );
        }

        /*
         * Obtener el PID del proceso actual.
         * La shell todavia no tiene PCB y devuelve 0.
         */
        case SYS_GET_PID: {
            Process *current = getCurrentProcess();
            return current != NULL ? current->pid : 0;
        }

        /*
         * Finalizar el proceso actual.
         * Si tiene exito, no retorna a Userland.
         */
        case SYS_EXIT:
            return exitCurrentProcess();

        /*
         * Terminar un proceso por PID.
         *
         * RDI = PID del proceso a terminar.
         * RAX = 1 si tuvo exito, 0 si fallo.
         *
         * Si el proceso se mata a si mismo,
         * debe ceder la CPU y no volver a ejecutarse.
         */
        case SYS_KILL: {
            uint64_t pid = frame->rdi;
            Process *current = getCurrentProcess();

            if (pid == 0) {
                return 0;
            }

            if (current != NULL && current->pid == pid) {
                return exitCurrentProcess();
            }

            uint8_t terminated = terminateProcess(pid);
            if (terminated) {
                semCancelWaiter(pid);
            }
            return terminated;
        }

        /*
        * Crear un semaforo con nombre.
        * RDI = nombre
        * RSI = valor inicial
        * RAX = ID o 0 si falla
        */
        case SYS_SEM_CREATE:
            return semCreate(
                (const char *)(uintptr_t)frame->rdi,
                frame->rsi
            );

        /*
        * Abrir un semaforo existente.
        * RDI = nombre
        * RAX = ID o 0 si falla
        */
        case SYS_SEM_OPEN:
            return semOpen(
                (const char *)(uintptr_t)frame->rdi
            );

        /*
        * Cerrar una apertura.
        * RDI = ID
        * RAX = 1 si tuvo exito, 0 si falla
        */
        case SYS_SEM_CLOSE:
            return semClose(frame->rdi);

        /* Esperar un recurso del semaforo. */
        case SYS_SEM_WAIT:
            return semWait(frame->rdi);

        /* Publicar un recurso del semaforo. */
        case SYS_SEM_POST:
            return semPost(frame->rdi);

        /*
         * Esperar la finalizacion de un hijo directo.
         *
         * RDI = PID del hijo.
         * RAX = PID del hijo si tuvo exito, 0 si fallo.
         *
         * Si el hijo sigue ejecutandose, waitForChild()
         * bloquea al padre y cede la CPU al scheduler.
         */
        case SYS_WAIT:
            return waitForChild(frame->rdi);

        /*
         * Copiar la tabla de procesos a Userland.
         * RDI = buffer de ProcessInfo
         * RSI = capacidad del buffer
         * RAX = cantidad de procesos copiados
         */
        case SYS_PS:
            return getProcessSnapshot(
                (ProcessInfo *)(uintptr_t)frame->rdi,
                frame->rsi
            );

        /*
         * Modificar la prioridad de un proceso.
         *
         * RDI = PID
         * RSI = nueva prioridad
         * RAX = 1 si tuvo exito, 0 si fallo
         */
        case SYS_NICE:
            return setProcessPriority(frame->rdi, frame->rsi);

        /*
         * Bloquear un proceso por PID.
         * RDI = PID
         * RAX = 1 si tuvo exito, 0 si fallo
         */
        case SYS_BLOCK:
            return blockProcess(frame->rdi);

        /*
         * Desbloquear un proceso por PID.
         * RDI = PID
         * RAX = 1 si tuvo exito, 0 si fallo
         */
        case SYS_UNBLOCK:
            return unblockProcess(frame->rdi);

        /*
         * Ceder voluntariamente la CPU.
         * INT 0x81 guarda el contexto y ejecuta el scheduler.
         */
        case SYS_YIELD:
#if !__STDC_HOSTED__
            __asm__ volatile ("int $0x81" ::: "memory");
#endif
            return 1;

        default:
            return 0;
    }
}
