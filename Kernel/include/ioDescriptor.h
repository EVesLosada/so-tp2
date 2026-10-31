#ifndef IO_DESCRIPTOR_H
#define IO_DESCRIPTOR_H

#include <stdint.h>

/*
 * Descriptores locales a cada proceso.
 */
#define MAX_PROCESS_DESCRIPTORS 16

#define IO_FD_STDIN  0
#define IO_FD_STDOUT 1

/*
 * Tipos de recursos que podra representar un descriptor.
 * PIPE queda reservado para los siguientes bloques.
 */
typedef enum {
    IO_DESCRIPTOR_UNUSED = 0,
    IO_DESCRIPTOR_KEYBOARD,
    IO_DESCRIPTOR_TERMINAL,
    IO_DESCRIPTOR_PIPE
} IODescriptorType;

/*
 * Permisos de lectura y escritura.
 */
#define IO_ACCESS_READ  0x01
#define IO_ACCESS_WRITE 0x02

/*
 * Entrada de la tabla de descriptores.
 *
 * resourceId identificara el recurso subyacente.
 * Para teclado y terminal se utiliza 0.
 */
typedef struct {
    uint8_t inUse;
    uint8_t permissions;
    IODescriptorType type;
    uint64_t resourceId;
} IODescriptor;


/*
 * Administracion de tablas de descriptores.
 *
 * ioGetCurrentDescriptorTable utiliza la tabla del PCB actual.
 * Cuando no existe un proceso actual, utiliza la de la shell.
 */
IODescriptor *ioGetCurrentDescriptorTable(void);

uint8_t ioGetDescriptor(
    const IODescriptor *table,
    uint64_t fd,
    IODescriptor *result
);

uint8_t ioSetDescriptor(
    IODescriptor *table,
    uint64_t fd,
    IODescriptor descriptor
);

/*
 * Operaciones de entrada y salida del contexto actual.
 */
uint64_t ioRead(uint64_t fd, char *buffer, uint64_t count);
uint64_t ioWrite(uint64_t fd, const char *buffer, uint64_t count);

/*
 * Reserva un descriptor libre desde la posicion 2.
 * Devuelve MAX_PROCESS_DESCRIPTORS si no hay espacio.
 */
uint64_t ioAllocateDescriptor(
    IODescriptor *table,
    IODescriptor descriptor
);

/*
 * Libera solo la entrada local.
 * No cierra ni libera el recurso subyacente.
 */
uint8_t ioUnmapDescriptor(IODescriptor *table, uint64_t fd);

#endif
