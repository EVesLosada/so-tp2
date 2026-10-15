include Makefile.inc

KERNEL=kernel.bin
# Administrador de memoria seleccionado.
# Por defecto se utiliza Free List.
MEMORY_MANAGER ?= freelist

ifeq ($(MEMORY_MANAGER),freelist)
MEMORY_MANAGER_SOURCE := memoryManager.c
else ifeq ($(MEMORY_MANAGER),buddy)
MEMORY_MANAGER_SOURCE := allocators/buddyMemoryManager.c
else
$(error MEMORY_MANAGER debe ser freelist o buddy)
endif
SOURCES=$(filter-out memoryManager.c,$(wildcard *.c)) \
        $(MEMORY_MANAGER_SOURCE) \
        $(wildcard drivers/*.c) \
        $(wildcard idt/*.c) \
        $(wildcard syscalls/*.c) \
        $(wildcard time/*.c) \
        $(wildcard cracker/*.c)

SOURCES_ASM=$(wildcard asm/*.asm)

OBJECTS=$(SOURCES:.c=.o)
OBJECTS_ASM=$(SOURCES_ASM:.asm=.o)

LOADERSRC=loader.asm
LOADEROBJECT=$(LOADERSRC:.asm=.o)
STATICLIBS=

all: $(KERNEL)

$(KERNEL): $(LOADEROBJECT) $(OBJECTS) $(STATICLIBS) $(OBJECTS_ASM) FORCE
	$(LD) $(LDFLAGS) -T kernel.ld -o $(KERNEL) $(LOADEROBJECT) $(OBJECTS) $(OBJECTS_ASM) $(STATICLIBS)

%.o: %.c
	$(GCC) $(GCCFLAGS) -I./include -c $< -o $@

%.o: %.asm
	$(ASM) $(ASMFLAGS) $< -o $@

$(LOADEROBJECT):
	$(ASM) $(ASMFLAGS) $(LOADERSRC) -o $(LOADEROBJECT)

clean:
	rm -rf *.o *.bin
	rm -rf drivers/*.o idt/*.o syscalls/*.o time/*.o cracker/*.o asm/*.o
	rm -f allocators/*.o

.PHONY: all clean FORCE
