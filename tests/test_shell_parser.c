#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <shellParser.h>

int main(void) {
    ShellParsedCommand parsed;

    /* Linea vacia. */
    assert(shellParseCommand("", &parsed));
    assert(parsed.argc == 0);

    /* Comando sin argumentos. */
    assert(shellParseCommand("echo", &parsed));
    assert(parsed.argc == 1);
    assert(strcmp(parsed.argv[0], "echo") == 0);
    assert(parsed.background == 0);

    /* Argumentos y espacios adicionales. */
    assert(shellParseCommand(
        "  echo   hola mundo  ", &parsed
    ));

    assert(parsed.argc == 3);
    assert(strcmp(parsed.argv[0], "echo") == 0);
    assert(strcmp(parsed.argv[1], "hola") == 0);
    assert(strcmp(parsed.argv[2], "mundo") == 0);

    /* Background separado por espacios. */
    assert(shellParseCommand(
        "echo hola mundo &", &parsed
    ));

    assert(parsed.argc == 3);
    assert(parsed.background == 1);

    /* Background pegado al comando. */
    assert(shellParseCommand("echo&", &parsed));
    assert(parsed.argc == 1);
    assert(parsed.background == 1);

    /* Sintaxis invalida. */
    assert(!shellParseCommand("&", &parsed));
    assert(!shellParseCommand("echo & otro", &parsed));
    assert(!shellParseCommand("echo &&", &parsed));
    /* Pipeline con espacios. */
    assert(shellParseCommand("cat | wc", &parsed));
    assert(parsed.argc == 1);
    assert(strcmp(parsed.argv[0], "cat") == 0);
    assert(parsed.pipeArgc == 1);
    assert(strcmp(parsed.pipeArgv[0], "wc") == 0);
    assert(parsed.background == 0);

    /* Pipeline sin espacios y con argumentos. */
    assert(shellParseCommand("echo hola|echo mundo", &parsed));
    assert(parsed.argc == 2);
    assert(strcmp(parsed.argv[0], "echo") == 0);
    assert(strcmp(parsed.argv[1], "hola") == 0);
    assert(parsed.pipeArgc == 2);
    assert(strcmp(parsed.pipeArgv[0], "echo") == 0);
    assert(strcmp(parsed.pipeArgv[1], "mundo") == 0);

    /* Pipeline en background. */
    assert(shellParseCommand("echo hola | echo mundo &", &parsed));
    assert(parsed.pipeArgc == 2);
    assert(parsed.background == 1);

    /* Los comandos simples no tienen segundo comando. */
    assert(shellParseCommand("echo hola", &parsed));
    assert(parsed.argc == 2);
    assert(parsed.pipeArgc == 0);

    /* Sintaxis invalida de pipelines. */
    assert(!shellParseCommand("| echo", &parsed));
    assert(!shellParseCommand("echo |", &parsed));
    assert(!shellParseCommand("echo || echo", &parsed));
    assert(!shellParseCommand("echo | | echo", &parsed));
    assert(!shellParseCommand("echo | echo | echo", &parsed));
    assert(!shellParseCommand("echo | &", &parsed));
    assert(!shellParseCommand("echo & | echo", &parsed));

    /* Exceso de argumentos. */
    assert(!shellParseCommand(
        "a b c d e f g h i j k l m n o p q",
        &parsed
    ));

    /* Exceso de longitud. */
    char tooLong[65];

    for (uint64_t i = 0; i < 64; i++) {
        tooLong[i] = 'x';
    }

    tooLong[64] = '\0';

    assert(!shellParseCommand(tooLong, &parsed));

    puts("OK: parser de comandos y argumentos");
    return 0;
}
