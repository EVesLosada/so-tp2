#include <shellParser.h>
#include <stddef.h>

static uint8_t isSpace(char c) {
    return c == ' ' || c == '\t';
}

uint8_t shellParseCommand(
    const char *line,
    ShellParsedCommand *result
) {
    if (line == NULL || result == NULL) {
        return 0;
    }

    *result = (ShellParsedCommand){0};

    uint64_t length = 0;

    while (length < SHELL_MAX_LINE_LENGTH &&
           line[length] != '\0') {
        result->buffer[length] = line[length];
        length++;
    }

    if (length == SHELL_MAX_LINE_LENGTH) {
        return 0;
    }

    result->buffer[length] = '\0';

    char *cursor = result->buffer;

    while (1) {
        while (isSpace(*cursor)) {
            cursor++;
        }

        if (*cursor == '\0') {
            return 1;
        }

        /*
         * '&' solo puede aparecer al final
         * y despues de al menos un comando.
         */
        if (*cursor == '&') {
            if (result->argc == 0) {
                return 0;
            }

            cursor++;

            while (isSpace(*cursor)) {
                cursor++;
            }

            if (*cursor != '\0') {
                return 0;
            }

            result->background = 1;
            return 1;
        }

        if (result->argc >= SHELL_MAX_ARGUMENTS) {
            return 0;
        }

        result->argv[result->argc++] = cursor;

        while (*cursor != '\0' &&
               !isSpace(*cursor) &&
               *cursor != '&') {
            /*
             * Los pipes se implementaran
             * en el bloque siguiente.
             */
            if (*cursor == '|') {
                return 0;
            }

            cursor++;
        }

        if (*cursor == '\0') {
            return 1;
        }

        if (*cursor == '&') {
            *cursor = '\0';
            cursor++;

            while (isSpace(*cursor)) {
                cursor++;
            }

            if (*cursor != '\0') {
                return 0;
            }

            result->background = 1;
            return 1;
        }

        *cursor = '\0';
        cursor++;
    }
}
