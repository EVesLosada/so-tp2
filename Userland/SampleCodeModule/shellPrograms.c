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
