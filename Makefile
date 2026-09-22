
.DEFAULT_GOAL := all

.PHONY: all toolchain bootloader kernel userland image clean docker

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


# Entorno de compilación obligatorio de SO
DOCKER_IMAGE := agodio/itba-so-multiarch:3.1

docker:
	docker run --rm -it \
		--user $(shell id -u):$(shell id -g) \
		-e HOME=/tmp \
		-v "$(CURDIR):/src" \
		-w /src \
		$(DOCKER_IMAGE) \
		bash