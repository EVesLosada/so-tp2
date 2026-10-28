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
GLOBAL sysNice
GLOBAL sysBlock
GLOBAL sysUnblock
GLOBAL sysYield

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
