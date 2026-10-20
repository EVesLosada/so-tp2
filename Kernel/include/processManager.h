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
