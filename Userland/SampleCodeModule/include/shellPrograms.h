#ifndef SHELL_PROGRAMS_H
#define SHELL_PROGRAMS_H

#include <stdint.h>

void shellEchoProcess(uint64_t argc, char **argv);
void shellCatProcess(uint64_t argc, char **argv);
void shellMemProcess(uint64_t argc, char **argv);
void shellPsProcess(uint64_t argc, char **argv);
void shellLoopProcess(uint64_t argc, char **argv);

#endif
