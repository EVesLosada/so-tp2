
.DEFAULT_GOAL := all

.PHONY: all toolchain bootloader kernel userland image clean

all: image

toolchain:
	$(MAKE) -C Toolchain all

bootloader:
	$(MAKE) -C Bootloader all

kernel:
	$(MAKE) -C Kernel all

userland:
	$(MAKE) -C Userland all

image: toolchain bootloader kernel userland
	$(MAKE) -C Image all

clean:
	$(MAKE) -C Bootloader clean
	$(MAKE) -C Image clean
	$(MAKE) -C Kernel clean
	$(MAKE) -C Userland clean
	$(MAKE) -C Toolchain clean