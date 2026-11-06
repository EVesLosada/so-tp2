#include <shellPrograms.h>
#include <syscalls.h>

/*
 * Programa echo.
 *
 * Imprime los argumentos recibidos y finaliza.
 * Puede ejecutarse en foreground o background.
 */
void shellEchoProcess(uint64_t argc, char **argv) {
    for (uint64_t i = 1; i < argc; i++) {
        if (i > 1) {
            sysWrite(STDOUT, " ", 1);
        }

        uint64_t length = 0;

        while (argv[i][length] != '\0') {
            length++;
        }

        sysWrite(STDOUT, argv[i], length);
    }

    sysWrite(STDOUT, "\n", 1);
}

/*
 * Programa cat.
 *
 * Copia STDIN a STDOUT hasta recibir EOF.
 * Sirve como consumidor de datos en un pipeline.
 */
void shellCatProcess(uint64_t argc, char **argv) {
    (void)argc;
    (void)argv;

    char buffer[128];
    uint64_t bytesRead;

    while ((bytesRead = sysRead(STDIN, buffer, sizeof(buffer))) > 0) {
        uint64_t written = 0;

        while (written < bytesRead) {
            uint64_t count = sysWrite(
                STDOUT,
                buffer + written,
                bytesRead - written
            );

            if (count == 0) {
                return;
            }

            written += count;
        }
    }
}
