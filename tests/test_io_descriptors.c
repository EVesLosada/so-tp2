#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <ioDescriptor.h>
#include <process.h>
#include <processManager.h>
#include <keyboard.h>
#include <terminal.h>

static Process *activeProcess = NULL;

static const char keyboardInput[] = "abc";
static uint64_t keyboardPosition = 0;

static char terminalOutput[64];
static uint64_t terminalLength = 0;

/* Mocks de los dispositivos y del proceso actual. */
Process *getCurrentProcess(void) {
    return activeProcess;
}

uint8_t keyboardReadChar(char *character) {
    if (character == NULL ||
        keyboardInput[keyboardPosition] == '\0') {
        return 0;
    }

    *character = keyboardInput[keyboardPosition++];
    return 1;
}

void terminalPutChar(char character) {
    assert(terminalLength < sizeof(terminalOutput));
    terminalOutput[terminalLength++] = character;
}

int main(void) {
    IODescriptor descriptor;

    /* 1. La shell tiene stdin y stdout. */
    IODescriptor *shell = ioGetCurrentDescriptorTable();

    assert(shell != NULL);
    assert(ioGetDescriptor(shell, IO_FD_STDIN, &descriptor));
    assert(descriptor.type == IO_DESCRIPTOR_KEYBOARD);
    assert(descriptor.permissions == IO_ACCESS_READ);

    assert(ioGetDescriptor(shell, IO_FD_STDOUT, &descriptor));
    assert(descriptor.type == IO_DESCRIPTOR_TERMINAL);
    assert(descriptor.permissions == IO_ACCESS_WRITE);

    /* 2. Lectura y escritura reales mediante los mocks. */
    char buffer[8] = {0};

    assert(ioWrite(IO_FD_STDOUT, "OK", 2) == 2);
    assert(terminalLength == 2);
    assert(terminalOutput[0] == 'O');
    assert(terminalOutput[1] == 'K');

    assert(ioRead(IO_FD_STDIN, buffer, 2) == 2);
    assert(buffer[0] == 'a' && buffer[1] == 'b');

    assert(ioRead(IO_FD_STDIN, buffer, 4) == 1);
    assert(buffer[0] == 'c');
    assert(ioRead(IO_FD_STDIN, buffer, 1) == 0);

    /* 3. No se permiten operaciones incompatibles. */
    assert(ioWrite(IO_FD_STDIN, "x", 1) == 0);
    assert(ioRead(IO_FD_STDOUT, buffer, 1) == 0);

    assert(ioWrite(IO_FD_STDOUT, NULL, 1) == 0);
    assert(ioRead(IO_FD_STDIN, NULL, 1) == 0);
    assert(ioRead(IO_FD_STDIN, buffer, 0) == 0);

    /* 4. No se puede acceder fuera de la tabla. */
    assert(!ioGetDescriptor(shell, MAX_PROCESS_DESCRIPTORS,
                            &descriptor));
    assert(!ioGetDescriptor(NULL, 0, &descriptor));
    assert(!ioGetDescriptor(shell, 0, NULL));

    /* 5. Dos procesos tienen tablas independientes. */
    Process first = {0};
    Process second = {0};

    first.ioDescriptors[IO_FD_STDIN] = shell[IO_FD_STDIN];
    first.ioDescriptors[IO_FD_STDOUT] = shell[IO_FD_STDOUT];

    second.ioDescriptors[IO_FD_STDIN] = shell[IO_FD_STDIN];
    second.ioDescriptors[IO_FD_STDOUT] = shell[IO_FD_STDOUT];

    activeProcess = &first;
    assert(ioGetCurrentDescriptorTable() == first.ioDescriptors);

    /* Agregar otra salida solo al primer proceso. */
    assert(ioSetDescriptor(
        first.ioDescriptors, 4,
        first.ioDescriptors[IO_FD_STDOUT]
    ));

    assert(ioWrite(4, "P", 1) == 1);
    assert(terminalLength == 3);

    activeProcess = &second;
    assert(ioGetCurrentDescriptorTable() == second.ioDescriptors);
    assert(!ioGetDescriptor(second.ioDescriptors, 4, &descriptor));
    assert(ioWrite(4, "Q", 1) == 0);
    assert(ioWrite(IO_FD_STDOUT, "B", 1) == 1);
    assert(terminalLength == 4);

    /* 6. Rechazar descriptores mal configurados. */
    IODescriptor invalid = {
        .inUse = 1,
        .permissions = IO_ACCESS_READ,
        .type = IO_DESCRIPTOR_TERMINAL,
        .resourceId = 0
    };

    assert(!ioSetDescriptor(second.ioDescriptors, 5, invalid));

    invalid.type = IO_DESCRIPTOR_PIPE;
    invalid.resourceId = 0;
    assert(!ioSetDescriptor(second.ioDescriptors, 5, invalid));

    invalid.resourceId = 7;
    invalid.permissions = IO_ACCESS_READ | IO_ACCESS_WRITE;
    assert(!ioSetDescriptor(second.ioDescriptors, 5, invalid));

    /* Un extremo valido puede representarse, aunque
       la lectura/escritura de pipes aun no exista. */
    invalid.permissions = IO_ACCESS_READ;
    assert(ioSetDescriptor(second.ioDescriptors, 5, invalid));
    assert(ioRead(5, buffer, 1) == 0);

    assert(!ioSetDescriptor(
        second.ioDescriptors, MAX_PROCESS_DESCRIPTORS, invalid
    ));

    /* 7. Volver a la shell no cambia su tabla. */
    activeProcess = NULL;
    assert(ioGetCurrentDescriptorTable() == shell);
    assert(!ioGetDescriptor(shell, 4, &descriptor));


    /* Reservar, liberar y reutilizar un descriptor. */
    IODescriptor outputDescriptor =
        second.ioDescriptors[IO_FD_STDOUT];

    assert(ioAllocateDescriptor(
        second.ioDescriptors, outputDescriptor
    ) == 2);

    assert(ioUnmapDescriptor(second.ioDescriptors, 2) == 1);
    assert(ioUnmapDescriptor(second.ioDescriptors, 2) == 0);

    assert(ioAllocateDescriptor(
        second.ioDescriptors, outputDescriptor
    ) == 2);

    assert(ioUnmapDescriptor(NULL, 2) == 0);
    assert(ioUnmapDescriptor(
        second.ioDescriptors, MAX_PROCESS_DESCRIPTORS
    ) == 0);

    puts("OK: permisos, aislamiento y operaciones de I/O");
    return 0;
}


/*
 * Stubs para el test unitario de descriptores.
 * La integracion real se prueba por separado.
 */
#include <pipeIO.h>

uint64_t pipeReadBlocking(
    uint64_t pipeId,
    uint8_t *destination,
    uint64_t count
) {
    (void)pipeId;
    (void)destination;
    (void)count;
    return 0;
}

uint64_t pipeWriteBlocking(
    uint64_t pipeId,
    const uint8_t *source,
    uint64_t count
) {
    (void)pipeId;
    (void)source;
    (void)count;
    return 0;
}
