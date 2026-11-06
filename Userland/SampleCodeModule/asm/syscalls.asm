global sysPipeOpen
global sysPipeCreate
%include "syscall_ids.inc"

GLOBAL sysWrite
GLOBAL sysRead
GLOBAL sysClear
GLOBAL sysFontUp
GLOBAL sysFontDown
GLOBAL sysGetRegisters
GLOBAL sysGetTicks
GLOBAL sysDrawRect
GLOBAL sysBeep
GLOBAL sysGetTime
GLOBAL sysSetHash
GLOBAL sysGetHash
GLOBAL sysCrack
GLOBAL sysMalloc
GLOBAL sysFree
GLOBAL sysMemInfo
GLOBAL sysCreateProcess
GLOBAL sysCreateProcessWithRedirectedIO
GLOBAL sysExit
GLOBAL sysKill
GLOBAL sysWait
GLOBAL sysGetPid
GLOBAL sysPs
GLOBAL sysNice
GLOBAL sysBlock
GLOBAL sysUnblock
GLOBAL sysYield
GLOBAL sysSemCreate
GLOBAL sysSemOpen
GLOBAL sysSemClose
GLOBAL sysSemWait
GLOBAL sysSemPost

section .text

; uint64_t sysNice(uint64_t pid, uint64_t priority)
; RDI = PID
; RSI = nueva prioridad
; uint64_t sysBlock(uint64_t pid)
sysBlock:
    mov rax, SYS_BLOCK
    int 0x80
    ret

; uint64_t sysUnblock(uint64_t pid)
; uint64_t sysYield(void)
sysYield:
    mov rax, SYS_YIELD
    int 0x80
    ret

sysUnblock:
    mov rax, SYS_UNBLOCK
    int 0x80
    ret

; uint64_t sysExit(void)
; Finaliza el proceso actual. No retorna si tiene exito.
; uint64_t sysKill(uint64_t pid)
; RDI = PID del proceso a terminar.
; RAX = 1 si tuvo exito, 0 si fallo.
; uint64_t sysWait(uint64_t childPid)
; RDI = PID del hijo que se desea esperar.
; RAX = PID del hijo o 0 si falla.
sysWait:
    mov rax, SYS_WAIT
    int 0x80
    ret

sysKill:
    mov rax, SYS_KILL
    int 0x80
    ret

sysExit:
    mov rax, SYS_EXIT
    int 0x80
    ret

; uint64_t sysCreateProcess(name, entryPoint, priority,
;                           foreground, argc, argv)
; Los seis argumentos llegan en RDI, RSI, RDX, RCX, R8 y R9.
sysCreateProcess:
    mov rax, SYS_CREATE_PROC
    int 0x80
    ret

; RDI = puntero a ProcessCreateIOArgs
; RAX = PID creado o 0 si falla
sysCreateProcessWithRedirectedIO:
    mov rax, SYS_PROC_REDIRECT
    int 0x80
    ret


sysPs:
    mov rax, SYS_PS
    int 0x80
    ret

sysGetPid:
    mov rax, SYS_GET_PID
    int 0x80
    ret

sysNice:
    mov rax, SYS_NICE
    int 0x80
    ret



; uint64_t sysWrite(uint64_t fd, const char *buffer, uint64_t count)
sysWrite:
    mov rax, SYS_WRITE
    int 0x80
    ret

; uint64_t sysRead(uint64_t fd, char *buffer, uint64_t count)
sysRead:
    mov rax, SYS_READ
    int 0x80
    ret

; uint64_t sysClear(void)
sysClear:
    mov rax, SYS_CLEAR
    int 0x80
    ret

; uint64_t sysFontUp(void)
sysFontUp:
    mov rax, SYS_FONT_UP
    int 0x80
    ret

; uint64_t sysFontDown(void)
sysFontDown:
    mov rax, SYS_FONT_DOWN
    int 0x80
    ret

; uint64_t sysGetRegisters(RegisterFrame *registers)
sysGetRegisters:
    mov rax, SYS_GET_REGS
    int 0x80
    ret

; uint64_t sysGetTicks(void)
sysGetTicks:
    mov rax, SYS_GET_TICKS
    int 0x80
    ret

; uint64_t sysDrawRect(uint32_t color, uint64_t x, uint64_t y, uint64_t width, uint64_t height)
sysDrawRect:
    mov rax, SYS_DRAW_RECT
    int 0x80
    ret

; uint64_t sysBeep(uint32_t frequency, uint64_t durationTicks)
sysBeep:
    mov rax, SYS_BEEP
    int 0x80
    ret

; uint64_t sysGetTime(uint8_t *hours, uint8_t *minutes, uint8_t *seconds)
;
; Argumentos:
;   RDI = hours
;   RSI = minutes
;   RDX = seconds
sysGetTime:
    mov rax, SYS_GET_TIME
    int 0x80
    ret

; uint64_t sysSetHash(uint64_t hash)
sysSetHash:
    mov rax, SYS_SET_HASH
    int 0x80
    ret

; uint64_t sysGetHash(uint64_t *hash)
sysGetHash:
    mov rax, SYS_GET_HASH
    int 0x80
    ret

; uint64_t sysCrack(char *resultBuffer,
;                   uint64_t resultBufferSize,
;                   uint64_t *elapsedCycles)
;
; Argumentos:
;   RDI = resultBuffer
;   RSI = resultBufferSize
;   RDX = elapsedCycles
sysCrack:
    mov rax, SYS_CRACK
    int 0x80
    ret

; void *sysMalloc(uint64_t size)
; RDI = cantidad de bytes solicitados
; RAX = puntero reservado o NULL
sysMalloc:
    mov rax, SYS_MALLOC
    int 0x80
    ret

; void sysFree(void *ptr)
; RDI = puntero a liberar
sysFree:
    mov rax, SYS_FREE
    int 0x80
    ret

; uint64_t sysMemInfo(MemoryInfo *info)
; RDI = destino de las estadisticas
; RAX = 1 si tuvo exito, 0 si fallo
sysMemInfo:
    mov rax, SYS_MEM_INFO
    int 0x80
    ret

; uint64_t sysSemCreate(const char *name, uint64_t initialValue)
; RDI = nombre, RSI = valor inicial
sysSemCreate:
    mov rax, SYS_SEM_CREATE
    int 0x80
    ret

; uint64_t sysSemOpen(const char *name)
; RDI = nombre
sysSemOpen:
    mov rax, SYS_SEM_OPEN
    int 0x80
    ret

; uint64_t sysSemClose(uint64_t id)
; RDI = ID
sysSemClose:
    mov rax, SYS_SEM_CLOSE
    int 0x80
    ret

; uint64_t sysSemWait(uint64_t id)
; RDI = ID del semaforo
sysSemWait:
    mov rax, SYS_SEM_WAIT
    int 0x80
    ret

; uint64_t sysSemPost(uint64_t id)
; RDI = ID del semaforo
sysSemPost:
    mov rax, SYS_SEM_POST
    int 0x80
    ret

; uint64_t sysPipeCreate(const char *name)
; RDI = nombre
sysPipeCreate:
    mov rax, SYS_PIPE_CREATE
    int 0x80
    ret

; uint64_t sysPipeOpen(const char *name, PipeMode mode)
; RDI = nombre, RSI = modo
sysPipeOpen:
    mov rax, SYS_PIPE_OPEN
    int 0x80
    ret

global sysPipeDestroy

; uint64_t sysPipeDestroy(uint64_t pipeId)
; RDI = ID del pipe
; RAX = 1 si tuvo exito, 0 si falla
sysPipeDestroy:
    mov rax, SYS_PIPE_DESTROY
    int 0x80
    ret

global sysClose

sysClose:
    mov rax, SYS_CLOSE
    int 0x80
    ret
