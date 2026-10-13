#ifndef MODULELOADER_H
#define MODULELOADER_H

#include <stdint.h>

/*
 * Carga los modulos y devuelve el limite superior
 * exclusivo de las regiones utilizadas:
 * origen empaquetado y destinos cargados.
 */
uint64_t loadModules(void *payloadStart,
                     void **moduleTargetAddress);

#endif
