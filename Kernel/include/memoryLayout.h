#ifndef MEMORY_LAYOUT_H
#define MEMORY_LAYOUT_H

#include <stdint.h>

/*
 * Mapa de memoria fisica utilizado por el sistema.
 *
 * Kernel:
 *   comienza en 0x100000, segun kernel.ld.
 *
 * Modulos:
 *   Userland se carga en 0x400000.
 *   El modulo de datos se carga en 0x500000.
 *
 * Las direcciones de los modulos representan regiones reservadas
 * por el sistema. No determinan por si solas el inicio o el fin
 * de la memoria administrable. Esa region debe seleccionarse
 * utilizando el mapa de memoria provisto por el bootloader.
 *
 * Estas constantes centralizan las regiones reservadas para evitar
 * que futuros administradores de memoria las utilicen accidentalmente.
 */

#define KERNEL_START_ADDRESS          0x100000UL

#define USERLAND_MODULE_ADDRESS       0x400000UL
#define DATA_MODULE_ADDRESS           0x500000UL

#define KERNEL_STACK_PAGES            8UL
#define PAGE_SIZE                     0x1000UL

#endif