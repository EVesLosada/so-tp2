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
    uint8_t afterPipe = 0;

    while (1) {
        while (isSpace(*cursor)) {
            cursor++;
        }

        if (*cursor == '\0') {
            return !afterPipe || result->pipeArgc > 0;
        }

        if (*cursor == '|') {
            if (afterPipe || result->argc == 0) {
                return 0;
            }

            afterPipe = 1;
            cursor++;
            continue;
        }

        if (*cursor == '&') {
            if (afterPipe
                    ? result->pipeArgc == 0
                    : result->argc == 0) {
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

        uint64_t *argc = afterPipe
            ? &result->pipeArgc
            : &result->argc;

        char **argv = afterPipe
            ? result->pipeArgv
            : result->argv;

        if (*argc >= SHELL_MAX_ARGUMENTS) {
            return 0;
        }

        argv[(*argc)++] = cursor;

        while (*cursor != '\0' &&
               !isSpace(*cursor) &&
               *cursor != '&' &&
               *cursor != '|') {
            cursor++;
        }

        if (*cursor == '\0') {
            return 1;
        }

        char separator = *cursor;
        *cursor = '\0';
        cursor++;

        if (separator == '|') {
            if (afterPipe) {
                return 0;
            }
            afterPipe = 1;
        } else if (separator == '&') {
            while (isSpace(*cursor)) {
                cursor++;
            }

            if (*cursor != '\0') {
                return 0;
            }

            result->background = 1;
            return 1;
        }
    }
}
