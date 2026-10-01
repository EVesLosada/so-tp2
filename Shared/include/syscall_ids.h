
#ifndef SYSCALL_IDS_H
#define SYSCALL_IDS_H

/*
 * Identificadores compartidos de las system calls.
 *
 * Kernel y Userland deben utilizar los mismos numeros.
 * Los identificadores existentes de Arqui se conservan
 * para mantener la compatibilidad.
 */

/* Entrada y salida */
#define SYS_WRITE       1
#define SYS_READ        2

/* Terminal */
#define SYS_CLEAR       3
#define SYS_FONT_UP     4
#define SYS_FONT_DOWN   5

/* Registros y tiempo */
#define SYS_GET_REGS    6
#define SYS_GET_TICKS   7

/* Video y sonido */
#define SYS_DRAW_RECT   8
#define SYS_BEEP        9

/* RTC */
#define SYS_GET_TIME   10

/* Funcionalidades heredadas de Arqui */
#define SYS_SET_HASH   11
#define SYS_GET_HASH   12
#define SYS_CRACK      13

#endif
