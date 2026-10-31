#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <stdint.h>
#include <process.h>

/*
 * Limites del subsistema de semaforos.
 */
#define MAX_SEMAPHORES 64
#define SEMAPHORE_NAME_MAX_LENGTH 32
#define SEMAPHORE_MAX_WAITERS MAX_PROCESSES

/*
 * Estructura interna de un semaforo.
 *
 * Las operaciones sobre esta estructura deben
 * ejecutarse dentro de una seccion critica.
 */
typedef struct Semaphore {
    /*
     * Indica si esta posicion de la tabla
     * contiene un semaforo valido.
     */
    uint8_t inUse;

    /*
     * Identificador numerico interno.
     * El valor 0 se reserva como invalido.
     */
    uint64_t id;

    /*
     * Nombre conocido por distintos procesos
     * para poder compartir el semaforo.
     */
    char name[SEMAPHORE_NAME_MAX_LENGTH];

    /*
     * Cantidad de recursos disponibles.
     */
    uint64_t value;

    /*
     * Cantidad de aperturas activas.
     */
    uint64_t openCount;

    /*
     * Cola circular FIFO de procesos que esperan.
     * Almacena PIDs, no punteros a PCB.
     */
    uint64_t waitingPids[SEMAPHORE_MAX_WAITERS];

    uint64_t waitingHead;
    uint64_t waitingTail;
    uint64_t waitingCount;

} Semaphore;

/*
 * Inicializacion y acceso a la tabla.
 */
void initializeSemaphoreTable(void);
Semaphore *getSemaphoreSlot(uint64_t index);

/*
 * Operaciones internas sobre la cola FIFO.
 *
 * Los PIDs deben corresponder a procesos validos
 * cuando se utilicen para bloquear/desbloquear.
 */
uint8_t enqueueSemaphoreWaiter(
    Semaphore *sem,
    uint64_t pid
);
uint64_t dequeueSemaphoreWaiter(Semaphore *sem);


/*
 * Administracion de semaforos con nombre.
 *
 * semCreate: devuelve un ID valido o 0 si falla.
 * semOpen: devuelve el ID existente o 0 si falla.
 * semClose: devuelve 1 si cierra correctamente, 0 si falla.
 */
uint64_t semCreate(const char *name, uint64_t initialValue);
uint64_t semOpen(const char *name);
uint8_t semClose(uint64_t id);
uint8_t semCancelWaiter(uint64_t pid);
uint8_t semWait(uint64_t id);
uint8_t semPost(uint64_t id);

#endif
