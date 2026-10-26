#include <stdint.h>
#include <interrupts.h>

#define DIVZERO_VECTOR       0x00
#define INVALID_OPCODE_VECTOR 0x06
#define TIMER_VECTOR          0x20
#define KEYBOARD_VECTOR        0x21
#define SYSCALL_VECTOR         0x80
#define YIELD_VECTOR           0x81

#define IDT_BASE_ADDRESS       0x0000000000000000
#define KERNEL_CODE_SELECTOR   0x08
#define KERNEL_INTERRUPT_GATE  0x8E
#define USER_INTERRUPT_GATE    0xEE

typedef struct {
    uint16_t offsetLow;
    uint16_t selector;
    uint8_t ist;
    uint8_t typeAttributes;
    uint16_t offsetMiddle;
    uint32_t offsetHigh;
    uint32_t zero;
} __attribute__((packed)) IDTEntry;

static volatile IDTEntry * const idt =
    (volatile IDTEntry *) IDT_BASE_ADDRESS;

extern void int80Handler(void);
extern void keyboardInterruptHandler(void);
extern void disableInterrupts(void);
extern void enableInterrupts(void);
extern void divZeroHandler(void);
extern void invalidOpcodeHandler(void);
extern void timerInterruptHandler(void);
extern void yieldInterruptHandler(void);
extern void enableTimerIRQ(void);

/*
 * Configura una entrada de la IDT para que apunte
 * al handler indicado.
 *
 * vector: numero de interrupcion/excepcion.
 * handler: rutina ASM que atiende el evento.
 * attributes: tipo de gate y nivel de privilegio.
 */
static void setIDTEntry(uint8_t vector, void (*handler)(void), uint8_t attributes) {
    uint64_t address = (uint64_t) handler;

    idt[vector].offsetLow = address & 0xFFFF;
    idt[vector].selector = KERNEL_CODE_SELECTOR;
    idt[vector].ist = 0;
    idt[vector].typeAttributes = attributes;
    idt[vector].offsetMiddle = (address >> 16) & 0xFFFF;
    idt[vector].offsetHigh = (address >> 32) & 0xFFFFFFFF;
    idt[vector].zero = 0;
}

void initializeInterrupts(void) {
    disableInterrupts();

    /*
     * Excepciones de CPU.
     */
    setIDTEntry(DIVZERO_VECTOR, divZeroHandler, KERNEL_INTERRUPT_GATE);
    setIDTEntry(INVALID_OPCODE_VECTOR, invalidOpcodeHandler, KERNEL_INTERRUPT_GATE);

    /*
     * IRQs de hardware.
     */
    setIDTEntry(TIMER_VECTOR, timerInterruptHandler, KERNEL_INTERRUPT_GATE);
    setIDTEntry(KEYBOARD_VECTOR, keyboardInterruptHandler, KERNEL_INTERRUPT_GATE);

    /*
     * Syscalls desde Userland.
     * Se usa un gate accesible desde ring 3.
     */
    setIDTEntry(SYSCALL_VECTOR, int80Handler, USER_INTERRUPT_GATE);

    /*
    * Interrupcion de software para ceder la CPU.
    * Solo accesible directamente desde codigo del kernel.
    */
    setIDTEntry(YIELD_VECTOR, yieldInterruptHandler, KERNEL_INTERRUPT_GATE);

    /*
     * Habilitamos IRQ0 en el PIC para que el timer pueda
     * llegar efectivamente al handler instalado en 0x20.
     */
    enableTimerIRQ();

    enableInterrupts();
}