#include <stdint.h>
#include <syscalls.h>

void shellWcProcess(uint64_t argc, char **argv) {
    (void)argv;

    if (argc != 1) {
        const char *msg = "Uso: wc\n";
        sysWrite(STDOUT, msg, 8);
        return;
    }

    char buffer[128];
    uint64_t lines = 0;
    uint64_t n;

    while ((n = sysRead(STDIN, buffer, sizeof(buffer))) > 0) {
        for (uint64_t i = 0; i < n; i++) {
            if (buffer[i] == '\n') {
                lines++;
            }
        }
    }

    char digits[21];
    uint64_t count = 0;

    do {
        digits[count++] = '0' + lines % 10;
        lines /= 10;
    } while (lines != 0);

    while (count > 0) {
        char c = digits[--count];
        sysWrite(STDOUT, &c, 1);
    }

    sysWrite(STDOUT, "\n", 1);
}
