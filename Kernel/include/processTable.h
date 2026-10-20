
#ifndef PROCESS_TABLE_H
#define PROCESS_TABLE_H

#include <stdint.h>
#include <process.h>

/*
 * Inicializa la tabla de procesos.
 *
 * Debe llamarse una vez al iniciar el
 * subsistema de procesos.
 */
void initializeProcessTable(void);

/*
 * Registra un proceso en la tabla.
 *
 * Retorna 1 si se pudo registrar y 0
 * si no hay lugar o el proceso es invalido.
 */
uint8_t registerProcess(Process *process);

/*
 * Busca un proceso por su PID.
 *
 * Retorna NULL si no existe.
 */
Process *findProcessByPid(uint64_t pid);

/*
 * Quita un proceso de la tabla.
 *
 * No libera su stack ni el PCB.
 * Esa responsabilidad corresponde al
 * administrador de procesos.
 *
 * Retorna 1 si lo encontro y 0 si no.
 */
uint8_t unregisterProcess(uint64_t pid);

/*
 * Devuelve el proceso ubicado en una posicion de la tabla.
 *
 * Retorna NULL si el indice es invalido
 * o si la posicion esta vacia.
 */
Process *getProcessAt(uint64_t index);

/*
 * Cantidad de procesos actualmente
 * registrados en la tabla.
 */
uint64_t getProcessCount(void);

#endif
