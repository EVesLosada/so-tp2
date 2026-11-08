#include <stdint.h>
#include <syscalls.h>

static uint8_t isVowel(char c) {
    return c == 'a' || c == 'e' || c == 'i' ||
           c == 'o' || c == 'u' ||
           c == 'A' || c == 'E' || c == 'I' ||
           c == 'O' || c == 'U';
}

void shellFilterProcess(uint64_t argc, char **argv) {
    (void)argv;

    if (argc != 1) {
        sysWrite(STDOUT, "Uso: filter\n", 12);
        return;
    }

    char input[128];
    char output[128];
    uint64_t n;

    while ((n = sysRead(STDIN, input, sizeof(input))) > 0) {
        uint64_t outputCount = 0;

        for (uint64_t i = 0; i < n; i++) {
            if (!isVowel(input[i])) {
                output[outputCount++] = input[i];
            }
        }

        uint64_t written = 0;
        while (written < outputCount) {
            uint64_t count = sysWrite(
                STDOUT, output + written, outputCount - written
            );

            if (count == 0) {
                return;
            }

            written += count;
        }
    }
}
