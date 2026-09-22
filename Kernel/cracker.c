#include <stdint.h>
#include <cracker.h>

/*
 * Hash objetivo almacenado en kernel.
 *
 * Userland no accede directamente a esta variable.
 * Solo puede modificarla o leerla mediante syscalls.
 */
static uint64_t targetHash = 0;
static uint8_t hasTargetHash = 0;

/*
 * Alfabeto pedido por el enunciado:
 * letras minusculas de la a a la z.
 */
static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz";
#define ALPHABET_SIZE 26

/*
 * Funcion ASM que lee el registro TSC mediante rdtsc.
 */
extern uint64_t readTSC(void);

void crackerSetHash(uint64_t hash) {
    targetHash = hash;
    hasTargetHash = 1;
}

uint8_t crackerGetHash(uint64_t *hash) {
    if (hash == 0 || !hasTargetHash) {
        return 0;
    }

    *hash = targetHash;
    return 1;
}

/*
 * Aplica un paso de la funcion hash del enunciado.
 *
 * hash = hash * 33 + c
 *      = ((hash << 5) + hash) + c
 */
static uint64_t hashStep(uint64_t currentHash, char c) {
    return ((currentHash << 5) + currentHash) + (uint8_t)c;
}

uint64_t crackerHash(const char *text) {
    uint64_t hash = 5381;
    uint64_t i = 0;

    while (text[i] != 0) {
        hash = hashStep(hash, text[i]);
        i++;
    }

    return hash;
}

/*
 * Copia una password encontrada al buffer de salida.
 */
static void copyPassword(char *destination,
                         const char *source,
                         uint64_t destinationSize) {
    uint64_t i = 0;

    if (destinationSize == 0) {
        return;
    }

    while (source[i] != 0 && i + 1 < destinationSize) {
        destination[i] = source[i];
        i++;
    }

    destination[i] = 0;
}

uint8_t crackerCrack(char *resultBuffer,
                     uint64_t resultBufferSize,
                     uint64_t *elapsedCycles) {
    char candidate[CRACKER_RESULT_SIZE];
    uint64_t startCycles;
    uint64_t endCycles;

    if (!hasTargetHash ||
        resultBuffer == 0 ||
        resultBufferSize < CRACKER_RESULT_SIZE ||
        elapsedCycles == 0) {
        return 0;
    }

    candidate[CRACKER_PASSWORD_LENGTH] = 0;

    startCycles = readTSC();

    for (uint64_t i0 = 0; i0 < ALPHABET_SIZE; i0++) {
        candidate[0] = alphabet[i0];
        uint64_t h1 = hashStep(5381, candidate[0]);

        for (uint64_t i1 = 0; i1 < ALPHABET_SIZE; i1++) {
            candidate[1] = alphabet[i1];
            uint64_t h2 = hashStep(h1, candidate[1]);

            for (uint64_t i2 = 0; i2 < ALPHABET_SIZE; i2++) {
                candidate[2] = alphabet[i2];
                uint64_t h3 = hashStep(h2, candidate[2]);

                for (uint64_t i3 = 0; i3 < ALPHABET_SIZE; i3++) {
                    candidate[3] = alphabet[i3];
                    uint64_t h4 = hashStep(h3, candidate[3]);

                    for (uint64_t i4 = 0; i4 < ALPHABET_SIZE; i4++) {
                        candidate[4] = alphabet[i4];
                        uint64_t h5 = hashStep(h4, candidate[4]);

                        if (h5 == targetHash) {
                            endCycles = readTSC();
                            *elapsedCycles = endCycles - startCycles;

                            copyPassword(
                                resultBuffer,
                                candidate,
                                resultBufferSize
                            );

                            return 1;
                        }
                    }
                }
            }
        }
    }

    endCycles = readTSC();
    *elapsedCycles = endCycles - startCycles;

    return 0;
}