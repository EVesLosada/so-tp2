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

section .text

; uint64_t sysWrite(uint64_t fd, const char *buffer, uint64_t count)
sysWrite:
    mov rax, 1
    int 0x80
    ret

; uint64_t sysRead(uint64_t fd, char *buffer, uint64_t count)
sysRead:
    mov rax, 2
    int 0x80
    ret

; uint64_t sysClear(void)
sysClear:
    mov rax, 3
    int 0x80
    ret

; uint64_t sysFontUp(void)
sysFontUp:
    mov rax, 4
    int 0x80
    ret

; uint64_t sysFontDown(void)
sysFontDown:
    mov rax, 5
    int 0x80
    ret

; uint64_t sysGetRegisters(RegisterFrame *registers)
sysGetRegisters:
    mov rax, 6
    int 0x80
    ret

; uint64_t sysGetTicks(void)
sysGetTicks:
    mov rax, 7
    int 0x80
    ret

; uint64_t sysDrawRect(uint32_t color, uint64_t x, uint64_t y, uint64_t width, uint64_t height)
sysDrawRect:
    mov rax, 8
    int 0x80
    ret

; uint64_t sysBeep(uint32_t frequency, uint64_t durationTicks)
sysBeep:
    mov rax, 9
    int 0x80
    ret

; uint64_t sysGetTime(uint8_t *hours, uint8_t *minutes, uint8_t *seconds)
;
; Argumentos:
;   RDI = hours
;   RSI = minutes
;   RDX = seconds

sysGetTime:
    mov rax, 10
    int 0x80
    ret 

; uint64_t sysSetHash(uint64_t hash)
sysSetHash:
    mov rax, 11
    int 0x80
    ret

; uint64_t sysGetHash(uint64_t *hash)
sysGetHash:
    mov rax, 12
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
    mov rax, 13
    int 0x80
    ret