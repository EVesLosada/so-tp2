# Tests auxiliares del TP2

Estas pruebas verifican componentes del kernel desde Ubuntu,
sin necesidad de iniciar QEMU.

## Ejecucion

Desde la raiz del repositorio ejecutar:

    ./tests/run_tests.sh

Se necesita un compilador C disponible como cc.

## Pruebas incluidas

- test_memory_region.c: seleccion de regiones E820.
- test_process_manager.c: creacion de procesos, PID, PCB y stacks.
- test_process_real_mm.c: creacion con Free List y Buddy.
- test_process_args.c: copia de argc/argv y validaciones.

Los ejecutables se generan en tmp/ y no se suben a GitHub.

## Alcance

Son pruebas auxiliares ejecutadas en Ubuntu.
No reemplazan las pruebas en QEMU ni los tests oficiales
test_mm, test_proc y test_sync.
