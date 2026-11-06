#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <process.h>
#include <processManager.h>
#include <processTable.h>
#include <memoryManager.h>
#include <ioDescriptor.h>
#include <pipe.h>
#include <pipeIO.h>
#include <pipeClose.h>

/*
 * Usamos los modulos reales del kernel.
 * Solo simulamos memoria y dispositivos.
 */
void *allocateMemory(uint64_t size) {
    return malloc((size_t)size);
}

void freeMemory(void *ptr) {
    free(ptr);
}

uint8_t keyboardReadChar(char *character) {
    (void)character;
    return 0;
}

void terminalPutChar(char character) {
    (void)character;
}

static void dummyEntry(void) {
}

/*
 * Abre un extremo con las funciones reales y mueve
 * el FD desde la tabla de la shell al PCB indicado.
 *
 * ioUnmapDescriptor no decrementa la referencia:
 * estamos transfiriendo su propiedad, no cerrandolo.
 */
static uint64_t openForProcess(
    const char *name,
    PipeMode mode,
    Process *owner
) {
    IODescriptor *shell = ioGetCurrentDescriptorTable();
    assert(shell != NULL);

    uint64_t shellFd = pipeOpenDescriptor(name, mode);
    assert(shellFd < MAX_PROCESS_DESCRIPTORS);

    IODescriptor descriptor;
    assert(ioGetDescriptor(shell, shellFd, &descriptor));

    uint64_t processFd =
        ioAllocateDescriptor(owner->ioDescriptors, descriptor);

    assert(processFd < MAX_PROCESS_DESCRIPTORS);
    assert(ioUnmapDescriptor(shell, shellFd) == 1);

    return processFd;
}

static void createPair(
    const char *name,
    Process **reader,
    Process **writer,
    Pipe **pipe
) {
    initializeProcessManager();
    initializePipeTable();

    uint64_t id = pipeCreate(name);
    assert(id != 0);

    uint64_t readerPid =
        createProcess("reader", dummyEntry, 1, 0, 0);

    uint64_t writerPid =
        createProcess("writer", dummyEntry, 1, 0, 0);

    assert(readerPid != 0);
    assert(writerPid != 0);

    *reader = findProcessByPid(readerPid);
    *writer = findProcessByPid(writerPid);
    *pipe = findPipeById(id);

    assert(*reader != NULL);
    assert(*writer != NULL);
    assert(*pipe != NULL);

    uint64_t readFd =
        openForProcess(name, PIPE_READ, *reader);

    uint64_t writeFd =
        openForProcess(name, PIPE_WRITE, *writer);

    assert(readFd == 2);
    assert(writeFd == 2);

    assert((*pipe)->readers == 1);
    assert((*pipe)->writers == 1);
}

static void finishAndReap(
    Process *reader,
    Process *writer
) {
    if (reader->state != PROCESS_TERMINATED) {
        assert(terminateProcess(reader->pid) == 1);
    }

    if (writer->state != PROCESS_TERMINATED) {
        assert(terminateProcess(writer->pid) == 1);
    }

    assert(reapTerminatedProcesses() == 2);
}

static void testKillLastWriter(void) {
    Process *reader;
    Process *writer;
    Pipe *pipe;

    createPair("kill-writer", &reader, &writer, &pipe);

    /*
     * El lector espera datos de un pipe vacio.
     */
    assert(blockProcess(reader->pid) == 1);

    reader->waitingPipeId = pipe->id;
    reader->waitingPipeMode = PIPE_READ;

    assert(pipeEnqueueWaiter(
        pipe, PIPE_READ, reader->pid
    ) == 1);

    assert(reader->state == PROCESS_BLOCKED);

    uint64_t writerPid = writer->pid;

    /*
     * El escritor muere sin ejecutar sysClose.
     * terminateProcess debe cerrar sus descriptores
     * y despertar al lector.
     */
    assert(terminateProcess(writerPid) == 1);

    assert(writer->state == PROCESS_TERMINATED);
    assert(writer->ioDescriptors[2].inUse == 0);

    assert(pipe->writers == 0);
    assert(pipe->readers == 1);

    assert(reader->state == PROCESS_READY);
    assert(reader->waitingPipeId == 0);
    assert(pipe->readerWaiters.count == 0);

    uint8_t output = 0;

    /* Buffer vacio y sin escritores: EOF. */
    assert(pipeReadBlocking(pipe->id, &output, 1) == 0);

    /* No puede finalizarse dos veces. */
    assert(terminateProcess(writerPid) == 0);

    finishAndReap(reader, writer);

    assert(pipe->readers == 0);
    assert(pipe->writers == 0);

    puts("OK: kill del ultimo escritor despierta lector y produce EOF");
}

static void testKillLastReader(void) {
    Process *reader;
    Process *writer;
    Pipe *pipe;

    createPair("kill-reader", &reader, &writer, &pipe);

    /*
     * Simular un escritor bloqueado esperando espacio.
     */
    assert(blockProcess(writer->pid) == 1);

    writer->waitingPipeId = pipe->id;
    writer->waitingPipeMode = PIPE_WRITE;

    assert(pipeEnqueueWaiter(
        pipe, PIPE_WRITE, writer->pid
    ) == 1);

    assert(writer->state == PROCESS_BLOCKED);

    /*
     * El lector muere sin cerrar explicitamente.
     */
    assert(terminateProcess(reader->pid) == 1);

    assert(reader->state == PROCESS_TERMINATED);
    assert(reader->ioDescriptors[2].inUse == 0);

    assert(pipe->readers == 0);
    assert(pipe->writers == 1);

    assert(writer->state == PROCESS_READY);
    assert(writer->waitingPipeId == 0);
    assert(pipe->writerWaiters.count == 0);

    uint8_t value = 'X';

    /* Sin lectores no debe aceptar mas escrituras. */
    assert(pipeWriteBlocking(pipe->id, &value, 1) == 0);

    finishAndReap(reader, writer);

    assert(pipe->readers == 0);
    assert(pipe->writers == 0);

    puts("OK: kill del ultimo lector despierta escritor");
}

static void testKillWaitingProcess(void) {
    Process *reader;
    Process *writer;
    Pipe *pipe;

    createPair("kill-waiter", &reader, &writer, &pipe);

    assert(blockProcess(reader->pid) == 1);

    reader->waitingPipeId = pipe->id;
    reader->waitingPipeMode = PIPE_READ;

    assert(pipeEnqueueWaiter(
        pipe, PIPE_READ, reader->pid
    ) == 1);

    /*
     * Esta vez muere el propio proceso bloqueado.
     * Debe salir de la cola antes de liberar su PCB.
     */
    assert(terminateProcess(reader->pid) == 1);

    assert(reader->state == PROCESS_TERMINATED);
    assert(reader->waitingPipeId == 0);
    assert(pipe->readerWaiters.count == 0);

    assert(pipe->readers == 0);
    assert(pipe->writers == 1);

    finishAndReap(reader, writer);

    assert(pipe->readers == 0);
    assert(pipe->writers == 0);

    puts("OK: kill elimina al proceso de su cola de espera");
}


static void testNormalExitClosesPipes(void) {
    Process *reader;
    Process *writer;
    Pipe *pipe;

    createPair("normal-exit", &reader, &writer, &pipe);

    /*
     * El escritor espera espacio. Lo bloqueamos
     * antes de seleccionar al lector.
     */
    assert(blockProcess(writer->pid) == 1);

    writer->waitingPipeId = pipe->id;
    writer->waitingPipeMode = PIPE_WRITE;

    assert(pipeEnqueueWaiter(
        pipe, PIPE_WRITE, writer->pid
    ) == 1);

    assert(writer->state == PROCESS_BLOCKED);

    /*
     * El scheduler selecciona al lector:
     * es el unico proceso READY.
     */
    uint64_t shellRsp = 0x98765000ULL;
    uint64_t readerRsp = schedule(shellRsp);

    assert(getCurrentProcess() == reader);
    assert(reader->state == PROCESS_RUNNING);
    assert(readerRsp == reader->savedStackPointer);

    /*
     * El lector termina normalmente sin sysClose.
     * Debe cerrar su FD y despertar al escritor.
     *
     * En Linux, exitCurrentProcess retorna porque
     * el cambio de contexto real esta deshabilitado.
     */
    assert(exitCurrentProcess() == 1);

    assert(reader->state == PROCESS_TERMINATED);
    assert(reader->ioDescriptors[2].inUse == 0);

    assert(pipe->readers == 0);
    assert(pipe->writers == 1);

    assert(writer->state == PROCESS_READY);
    assert(writer->waitingPipeId == 0);
    assert(pipe->writerWaiters.count == 0);

    /*
     * Todavia no podemos liberar el PCB del lector:
     * sigue siendo el proceso actual.
     */
    assert(reapTerminatedProcesses() == 0);

    /*
     * El scheduler puede seleccionar directamente
     * al escritor o volver primero a la shell.
     */
    uint64_t nextRsp = schedule(readerRsp);

    if (getCurrentProcess() == NULL) {
        assert(nextRsp == shellRsp);
        nextRsp = schedule(shellRsp);
    }

    assert(getCurrentProcess() == writer);
    assert(writer->state == PROCESS_RUNNING);
    assert(nextRsp == writer->savedStackPointer);

    /*
     * El escritor termina normalmente.
     * Su extremo tambien debe cerrarse.
     */
    uint64_t writerRsp = nextRsp;

    assert(exitCurrentProcess() == 1);

    assert(writer->state == PROCESS_TERMINATED);
    assert(writer->ioDescriptors[2].inUse == 0);
    assert(pipe->writers == 0);

    /*
     * Sin procesos READY, volvemos a la shell.
     */
    assert(schedule(writerRsp) == shellRsp);
    assert(getCurrentProcess() == NULL);

    /* Ahora ambos PCB pueden recolectarse. */
    assert(reapTerminatedProcesses() == 2);
    assert(getProcessCount() == 0);

    assert(pipe->readers == 0);
    assert(pipe->writers == 0);

    puts("OK: exit normal cierra pipes y despierta escritor");
    puts("OK: ambos procesos liberados despues del scheduler");
}

static void dummyEntryWithArgs(uint64_t argc, char **argv) {
    (void)argc;
    (void)argv;
}

static void testRedirectedProcessInheritance(void) {
    initializeProcessManager();
    initializePipeTable();

    uint64_t pipeId = pipeCreate("redirect-inherit");
    assert(pipeId != 0);

    Pipe *pipe = findPipeById(pipeId);
    assert(pipe != NULL);

    uint64_t readFd =
        pipeOpenDescriptor("redirect-inherit", PIPE_READ);
    uint64_t writeFd =
        pipeOpenDescriptor("redirect-inherit", PIPE_WRITE);

    assert(readFd < MAX_PROCESS_DESCRIPTORS);
    assert(writeFd < MAX_PROCESS_DESCRIPTORS);

    assert(pipe->readers == 1);
    assert(pipe->writers == 1);

    uint64_t readerPid = createProcessWithRedirectedIO(
        "redirect-reader",
        dummyEntryWithArgs,
        1, 0, 0, 0, NULL,
        readFd, IO_FD_STDOUT
    );

    assert(readerPid != 0);
    assert(pipe->readers == 2);
    assert(pipe->writers == 1);

    Process *reader = findProcessByPid(readerPid);
    assert(reader != NULL);
    assert(reader->ioDescriptors[IO_FD_STDIN].inUse);
    assert(reader->ioDescriptors[IO_FD_STDIN].type ==
           IO_DESCRIPTOR_PIPE);
    assert(reader->ioDescriptors[IO_FD_STDIN].resourceId ==
           pipeId);

    uint64_t writerPid = createProcessWithRedirectedIO(
        "redirect-writer",
        dummyEntryWithArgs,
        1, 0, 0, 0, NULL,
        IO_FD_STDIN, writeFd
    );

    assert(writerPid != 0);
    assert(pipe->readers == 2);
    assert(pipe->writers == 2);

    Process *writer = findProcessByPid(writerPid);
    assert(writer != NULL);
    assert(writer->ioDescriptors[IO_FD_STDOUT].inUse);
    assert(writer->ioDescriptors[IO_FD_STDOUT].type ==
           IO_DESCRIPTOR_PIPE);
    assert(writer->ioDescriptors[IO_FD_STDOUT].resourceId ==
           pipeId);

    assert(terminateProcess(readerPid) == 1);
    assert(pipe->readers == 1);
    assert(pipe->writers == 2);

    assert(terminateProcess(writerPid) == 1);
    assert(pipe->readers == 1);
    assert(pipe->writers == 1);

    assert(reapTerminatedProcesses() == 2);

    IODescriptor *shellTable = ioGetCurrentDescriptorTable();
    assert(shellTable != NULL);

    assert(pipeCloseDescriptor(shellTable, readFd) == 1);
    assert(pipeCloseDescriptor(shellTable, writeFd) == 1);

    assert(pipe->readers == 0);
    assert(pipe->writers == 0);
    assert(pipeDestroy(pipeId) == 1);

    puts("OK: herencia y liberacion de STDIN/STDOUT redirigidos");
}

static void testInvalidRedirectedDescriptors(void) {
    initializeProcessManager();
    initializePipeTable();

    uint64_t pipeId = pipeCreate("redirect-invalid");
    assert(pipeId != 0);

    Pipe *pipe = findPipeById(pipeId);
    assert(pipe != NULL);

    uint64_t readFd =
        pipeOpenDescriptor("redirect-invalid", PIPE_READ);
    uint64_t writeFd =
        pipeOpenDescriptor("redirect-invalid", PIPE_WRITE);

    assert(readFd < MAX_PROCESS_DESCRIPTORS);
    assert(writeFd < MAX_PROCESS_DESCRIPTORS);

    assert(pipe->readers == 1);
    assert(pipe->writers == 1);

    /* STDIN no puede recibir un extremo de escritura. */
    assert(createProcessWithRedirectedIO(
        "invalid-input", dummyEntryWithArgs,
        1, 0, 0, 0, NULL,
        writeFd, IO_FD_STDOUT
    ) == 0);

    /* STDOUT no puede recibir un extremo de lectura. */
    assert(createProcessWithRedirectedIO(
        "invalid-output", dummyEntryWithArgs,
        1, 0, 0, 0, NULL,
        IO_FD_STDIN, readFd
    ) == 0);

    /* El descriptor debe existir en la tabla del padre. */
    assert(createProcessWithRedirectedIO(
        "missing-input", dummyEntryWithArgs,
        1, 0, 0, 0, NULL,
        MAX_PROCESS_DESCRIPTORS, IO_FD_STDOUT
    ) == 0);

    /* Si falla STDOUT, debe revertirse la herencia de STDIN. */
    assert(createProcessWithRedirectedIO(
        "rollback-input", dummyEntryWithArgs,
        1, 0, 0, 0, NULL,
        readFd, MAX_PROCESS_DESCRIPTORS
    ) == 0);

    assert(getProcessCount() == 0);
    assert(pipe->readers == 1);
    assert(pipe->writers == 1);

    IODescriptor *shellTable = ioGetCurrentDescriptorTable();
    assert(shellTable != NULL);

    assert(pipeCloseDescriptor(shellTable, readFd) == 1);
    assert(pipeCloseDescriptor(shellTable, writeFd) == 1);

    assert(pipe->readers == 0);
    assert(pipe->writers == 0);
    assert(pipeDestroy(pipeId) == 1);

    puts("OK: redirecciones invalidas sin fugas de referencias");
}

int main(void) {
    testInvalidRedirectedDescriptors();
    testRedirectedProcessInheritance();
    testKillLastWriter();
    testKillLastReader();
    testKillWaitingProcess();
    testNormalExitClosesPipes();

    puts("OK: test_pipe_process_lifecycle");
    return 0;
}
