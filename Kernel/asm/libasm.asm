GLOBAL cpuVendor
GLOBAL int80Handler
GLOBAL keyboardInterruptHandler
GLOBAL disableInterrupts
GLOBAL enableInterrupts
GLOBAL divZeroHandler
GLOBAL invalidOpcodeHandler
GLOBAL timerInterruptHandler
GLOBAL enableTimerIRQ
GLOBAL outb
GLOBAL inb
GLOBAL readTSC
GLOBAL yieldInterruptHandler

EXTERN timerTick
EXTERN syscallDispatcher
EXTERN keyboardProcessScancode
EXTERN exceptionDispatcher
EXTERN schedule

section .text
	
cpuVendor:
	push rbp
	mov rbp, rsp

	push rbx

	mov rax, 0
	cpuid


	mov [rdi], ebx
	mov [rdi + 4], edx
	mov [rdi + 8], ecx

	mov byte [rdi + 13], 0

	mov rax, rdi

	pop rbx

	mov rsp, rbp
	pop rbp
	ret

; ---------------------------------------------------------------------------
; saveState / restoreState
;
; La CPU ya apilo RIP, CS y RFLAGS.
; Nosotros guardamos registros generales y agregamos el RSP original.
; ---------------------------------------------------------------------------

%macro saveState 0
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rsi
    push rdi
    push rbp
    push rdx
    push rcx
    push rbx
    push rax

    ; 15 registros guardados + RIP, CS y RFLAGS = 18 qwords.
    lea rax, [rsp + 18 * 8]
    push rax
%endmacro

%macro restoreState 0
    add rsp, 8

    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rbp
    pop rdi
    pop rsi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
%endmacro


; ---------------------------------------------------------------------------
; int80Handler
; Entrada de la API de syscalls.
; ---------------------------------------------------------------------------

int80Handler:
    saveState

    ; RDI recibe un puntero al RegisterFrame.
    mov rdi, rsp

    ; En este camino el stack queda alineado para llamar codigo C.
    call syscallDispatcher

    ; Offset 0: rsp sintetico. Offset 8: rax guardado.
    mov [rsp + 8], rax

    restoreState
    iretq

; ---------------------------------------------------------------------------
; INT 0x81: cambio de contexto voluntario.
;
; Permite que un proceso ceda la CPU inmediatamente.
; No incrementa ticks ni envia EOI al PIC porque
; no corresponde a una interrupcion de hardware.
; ---------------------------------------------------------------------------

yieldInterruptHandler:
    saveState

    ; Guardar el contexto actual.
    mov r12, rsp

    ; Alinear el stack antes de llamar a C.
    and rsp, -16

    ; Elegir el siguiente contexto.
    mov rdi, r12
    call schedule

    ; Restaurar el contexto seleccionado.
    mov rsp, rax

    restoreState
    iretq

; ---------------------------------------------------------------------------
; IRQ1 / INT 0x21: handler de teclado.
; ---------------------------------------------------------------------------

keyboardInterruptHandler:
    saveState

    ; El controlador de teclado entrega el scancode en el puerto 0x60.
    xor rax, rax
    in al, 0x60

    ; Guardamos el puntero al RegisterFrame.
    mov r12, rsp

    ; Primer argumento: scancode.
    movzx rdi, al

    ; Segundo argumento: frame de registros.
    mov rsi, r12

    ; Alineamos stack antes de llamar C.
    and rsp, -16
    call keyboardProcessScancode
    mov rsp, r12

    ; End Of Interrupt al PIC master.
    mov al, 0x20
    out 0x20, al

    restoreState
    iretq


; ---------------------------------------------------------------------------
; Habilitacion/deshabilitacion global de interrupciones externas.
; ---------------------------------------------------------------------------

disableInterrupts:
    cli
    ret

enableInterrupts:
    sti
    ret

; ---------------------------------------------------------------------------
; Division by zero
; ---------------------------------------------------------------------------

divZeroHandler:
    saveState

    mov r12, rsp

    mov rdi, 0
    mov rsi, r12

    and rsp, -16
    call exceptionDispatcher
    mov rsp, r12

    ; Saltear la instruccion div rcx que genero la excepcion.
    ; En nuestro triggerDivZero, "div %rcx" mide 3 bytes.
    add qword [rsp + 16 * 8], 3

    restoreState
    iretq

; ---------------------------------------------------------------------------
; Invalid opcode
; ---------------------------------------------------------------------------

invalidOpcodeHandler:
    saveState

    mov r12, rsp

    mov rdi, 6
    mov rsi, r12

    and rsp, -16
    call exceptionDispatcher
    mov rsp, r12

    ; Saltear la instruccion ud2, que mide 2 bytes.
    add qword [rsp + 16 * 8], 2

    restoreState
    iretq

; ---------------------------------------------------------------------------
; IRQ0 / INT 0x20: timer.
;
; Cada interrupcion del timer incrementa un contador global
; que despues puede consultar Userland mediante SYS_GET_TICKS.
; ---------------------------------------------------------------------------

timerInterruptHandler:
    saveState

    ; Guardamos el RSP del contexto interrumpido.
    mov r12, rsp

    ; Alineamos el stack para llamar a funciones C.
    and rsp, -16

    ; Actualizamos los ticks y el sonido.
    call timerTick

    ; schedule(savedRsp) recibe el contexto interrumpido.
    mov rdi, r12
    call schedule

    ; RAX contiene el RSP del contexto seleccionado.
    mov rsp, rax

    ; Confirmamos la interrupcion al PIC master.
    mov al, 0x20
    out 0x20, al

    ; Restauramos el contexto seleccionado.
    restoreState
    iretq

; ---------------------------------------------------------------------------
; Habilita IRQ0 en el PIC master.
;
; IRQ0 corresponde al timer. El PIC usa una mascara:
; bit = 1 -> IRQ bloqueada
; bit = 0 -> IRQ habilitada
;
; Limpiamos el bit 0 del puerto 0x21 para permitir
; que lleguen interrupciones del timer al vector 0x20.
; ---------------------------------------------------------------------------

enableTimerIRQ:
    in al, 0x21
    and al, 0xFE
    out 0x21, al
    ret

; ---------------------------------------------------------------------------
; Escritura de un byte a un puerto de E/S.
;
; Argumentos:
;   RDI = puerto
;   RSI = valor
; ---------------------------------------------------------------------------

outb:
    mov dx, di
    mov al, sil
    out dx, al
    ret


; ---------------------------------------------------------------------------
; Lectura de un byte desde un puerto de E/S.
;
; Argumentos:
;   RDI = puerto
;
; Retorno:
;   RAX = valor leido
; ---------------------------------------------------------------------------

inb:
    mov dx, di
    xor rax, rax
    in al, dx
    ret

; ---------------------------------------------------------------------------
; Lee el Time Stamp Counter.
;
; Retorna:
;   RAX = valor de 64 bits del contador TSC
; ---------------------------------------------------------------------------

readTSC:
    rdtsc
    shl rdx, 32
    or rax, rdx
    ret