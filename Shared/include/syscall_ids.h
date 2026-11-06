
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

/* Memoria */
#define SYS_MALLOC       14
#define SYS_FREE         15
#define SYS_MEM_INFO     16

/* Procesos */
#define SYS_CREATE_PROC  17
#define SYS_EXIT         18
#define SYS_GET_PID      19
#define SYS_PS           20
#define SYS_KILL         21
#define SYS_NICE         22
#define SYS_BLOCK        23
#define SYS_UNBLOCK      24
#define SYS_YIELD        25
#define SYS_WAIT         26

/* Semaforos */
#define SYS_SEM_CREATE   27
#define SYS_SEM_OPEN     28
#define SYS_SEM_CLOSE    29
#define SYS_SEM_WAIT     30
#define SYS_SEM_POST     31

/* Pipes */
#define SYS_PIPE_CREATE  32
#define SYS_PIPE_OPEN    33
#define SYS_CLOSE        34

/* Redireccion de descriptores de procesos hijos */
#define SYS_PROC_REDIRECT 35

/* Eliminar un pipe sin extremos abiertos ni procesos esperando */
#define SYS_PIPE_DESTROY 36

#endif
