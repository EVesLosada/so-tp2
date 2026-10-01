%define SYS_WRITE       1
%define SYS_READ        2
%define SYS_CLEAR       3
%define SYS_FONT_UP     4
%define SYS_FONT_DOWN   5
%define SYS_GET_REGS    6
%define SYS_GET_TICKS   7
%define SYS_DRAW_RECT   8
%define SYS_BEEP        9
%define SYS_GET_TIME    10
%define SYS_SET_HASH    11
%define SYS_GET_HASH    12
%define SYS_CRACK       13

; Memoria
%define SYS_MALLOC       14
%define SYS_FREE         15
%define SYS_MEM_INFO     16

; Procesos
%define SYS_CREATE_PROC  17
%define SYS_EXIT         18
%define SYS_GET_PID      19
%define SYS_PS           20
%define SYS_KILL         21
%define SYS_NICE         22
%define SYS_BLOCK        23
%define SYS_UNBLOCK      24
%define SYS_YIELD        25
%define SYS_WAIT         26

; Semaforos
%define SYS_SEM_CREATE   27
%define SYS_SEM_OPEN     28
%define SYS_SEM_CLOSE    29
%define SYS_SEM_WAIT     30
%define SYS_SEM_POST     31

; Pipes
%define SYS_PIPE_CREATE  32
%define SYS_PIPE_OPEN    33
%define SYS_CLOSE        34