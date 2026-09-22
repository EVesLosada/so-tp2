#ifndef CRACKER_H
#define CRACKER_H

#include <stdint.h>

#define CRACKER_PASSWORD_LENGTH 5
#define CRACKER_RESULT_SIZE 6

/*
 * Guarda el hash objetivo que luego intentara resolver el cracker.
 */
void crackerSetHash(uint64_t hash);

/*
 * Obtiene el hash objetivo guardado.
 *
 * Retorna:
 *   1 si habia un hash cargado
 *   0 si todavia no se cargo ninguno
 */
uint8_t crackerGetHash(uint64_t *hash);

/*
 * Funcion hash provista por la catedra.
 *
 * hash inicial = 5381
 * hash = hash * 33 + c
 */
uint64_t crackerHash(const char *text);

/*
 * Intenta encontrar una password de 5 letras minusculas
 * cuyo hash coincida con el hash objetivo guardado.
 *
 * resultBuffer: buffer donde se copia la password encontrada.
 * resultBufferSize: tamanio del buffer.
 * elapsedCycles: ciclos de CPU insumidos medidos con rdtsc.
 *
 * Retorna:
 *   1 si encontro password
 *   0 si no encontro o no habia hash cargado
 */
uint8_t crackerCrack(char *resultBuffer,
                     uint64_t resultBufferSize,
                     uint64_t *elapsedCycles);

#endif