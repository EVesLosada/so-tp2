#include <stdint.h>
#include <stdint.h>
#include <process.h>

/*
 * Dependencias nuevas de ioDescriptor.c para Ctrl+D.
 *
 * Son implementaciones debiles: los tests que ya
 * proporcionan una funcion real o simulada conservan
 * esa version. No se enlazan en el kernel final.
 */
__attribute__((weak))
Process *getProcessAt(uint64_t index) {
    (void)index;
    return 0;
}

__attribute__((weak))
uint8_t blockProcess(uint64_t pid) {
    (void)pid;
    return 0;
}

__attribute__((weak))
void keyboardSetWaitingProcess(uint64_t pid) {
    (void)pid;
}

/* Stub para tests que no enlazan el subsistema real de semaforos. */
__attribute__((weak))
uint8_t semCancelWaiter(uint64_t pid) {
    (void)pid;
    return 0;
}
