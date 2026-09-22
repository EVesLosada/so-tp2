# Trabajo Práctico 2 — Sistemas Operativos
Construcción del núcleo de un sistema operativo de 64 bits y mecanismos de administración de recursos.
El proyecto utiliza como punto de partida el trabajo práctico de Arquitectura de Computadoras, basado en x64BareBones.

## Integrantes
* Elena Ves Losada
* Candela Francavilla
* Natala Besuschio

## Decisiones de diseño

*Pendiente de completar.*

## Compilación y ejecución

### Requisitos

* Docker.
* GNU Make.
* QEMU (`qemu-system-x86_64`).

La compilación debe realizarse utilizando la imagen Docker provista por la cátedra:

`agodio/itba-so-multiarch:3.1`

### Compilación

Desde la raíz del proyecto, iniciar el entorno de compilación:

```bash
make docker
```

Dentro del contenedor, compilar el proyecto:

```bash
make
```

También puede utilizarse `make all`.

Para eliminar los archivos generados durante la compilación:

```bash
make clean
```

### Ejecución

Salir del contenedor:

```bash
exit
```

Desde el sistema anfitrión, ejecutar:

```bash
make run
```

Este comando inicia QEMU utilizando la imagen de arranque generada durante la compilación.

## Limitaciones

*Pendiente de completar.*

## Problemas encontrados y soluciones

*Pendiente de completar.*

## Citas de código y uso de IA

*Pendiente de completar.*

## Instrucciones de replicación

### Comandos y tests

*Pendiente de completar: nombre, descripción y parámetros de cada comando y test.*

### Caracteres especiales

*Pendiente de completar: caracteres utilizados para pipes y ejecución de comandos en background.*

### Atajos de teclado

*Pendiente de completar: atajos para interrumpir la ejecución y enviar EOF.*

### Ejemplos de funcionamiento

*Pendiente de completar: ejemplos para demostrar el funcionamiento de cada requerimiento, por fuera de los tests provistos.*

## Créditos

Este proyecto utiliza como base x64BareBones.

**Autor original:** Rodrigo Rearden (RowDaBoat).

**Colaborador:** Augusto Nizzo McIntosh.

Se conserva el archivo `License.txt` correspondiente al proyecto original.
