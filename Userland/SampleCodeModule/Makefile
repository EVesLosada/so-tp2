include ../Makefile.inc

MODULE=0000-sampleCodeModule.bin

SOURCES_C=$(wildcard [^_]*.c)
SOURCES_ASM=$(wildcard asm/*.asm)
OBJECTS_ASM=$(SOURCES_ASM:.asm=.o)

all: $(MODULE)

%.o: %.asm
	$(ASM) $(ASMFLAGS) $< -o $@

$(MODULE): $(SOURCES_C) $(OBJECTS_ASM)
	$(GCC) $(GCCFLAGS) -I./include -T sampleCodeModule.ld _loader.c $(SOURCES_C) $(OBJECTS_ASM) -o ../$(MODULE)

clean:
	rm -rf *.o asm/*.o

.PHONY: all clean