#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <stdint.h>
#include <process.h>

/*
 * Inicializa el subsistema de procesos.
 * Debe llamarse una vez, despues del Memory Manager.
 */
void initializeProcessManager(void);

/*
 * Crea un proceso y lo registra como PROCESS_READY.
 *
 * Retorna:
 *   PID > 0 si tuvo exito.
 *   0 si no pudo crear el proceso.
 */
uint64_t createProcess(const char *name,
                       void (*entryPoint)(void),
                       uint64_t priority,
                       uint64_t parentPid,
                       uint8_t foreground);

/*
 * Crea un proceso con argumentos.
 *
 * El proceso conservara una copia de argv,
 * independiente de los argumentos originales.
 *
 * Retorna el PID creado o 0 si falla.
 */
uint64_t createProcessWithArgs(
    const char *name,
    void (*entryPoint)(uint64_t argc, char **argv),
    uint64_t priority,
    uint64_t parentPid,
    uint8_t foreground,
    uint64_t argc,
    const char *const argv[]
);

/*
 * Busca el siguiente proceso listo para ejecutar.
 *
 * Recorre la tabla de forma circular.
 * Retorna NULL si no hay procesos READY.
 *
 * No modifica los estados de los procesos.
 */
Process *selectNextReadyProcess(void);

/*
 * Devuelve el proceso actualmente en ejecucion.
 * Retorna NULL si todavia no hay uno.
 */
Process *getCurrentProcess(void);

/*
 * Bloquea un proceso READY.
 *
 * Si el proceso esta actualmente en RUNNING,
 * lo bloquea y cede la CPU mediante int 0x81.
 *
 * Retorna 1 si tuvo exito y 0 si no.
 */
uint8_t blockProcess(uint64_t pid);

/*
 * Desbloquea un proceso y lo deja listo para ejecutar.
 *
 * Retorna 1 si tuvo exito y 0 si no.
 */
uint8_t unblockProcess(uint64_t pid);

/*
 * Marca como terminado un proceso READY o BLOCKED.
 *
 * No permite terminar directamente un proceso RUNNING.
 * La liberacion de sus recursos se realizara despues.
 *
 * Retorna 1 si tuvo exito y 0 si no.
 */
uint8_t terminateProcess(uint64_t pid);

/*
 * Libera los recursos de los procesos terminados
 * que ya no estan ejecutandose.
 *
 * Retorna la cantidad de procesos eliminados.
 */
uint64_t reapTerminatedProcesses(void);

/*
 * Espera la finalizacion de un hijo directo.
 *
 * Si el hijo sigue ejecutandose, bloquea al padre.
 * Si ya termino, recoge su finalizacion y libera
 * los recursos del hijo.
 *
 * Retorna el PID del hijo si tuvo exito.
 * Retorna 0 si el PID es invalido o no es su hijo.
 */
uint64_t waitForChild(uint64_t childPid);

/**
 * @brief Selecciona el contexto que debe ejecutarse a continuación.
 *
 * Recibe el puntero al stack del contexto interrumpido y devuelve
 * el puntero al stack del contexto que se debe restaurar.
 *
 * @param savedRsp Puntero al stack con los registros guardados.
 * @return Puntero al stack del contexto seleccionado.
 */
uint64_t schedule(uint64_t savedRsp);

#endif
