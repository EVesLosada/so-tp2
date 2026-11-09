# TP2 — Sistemas Operativos

**Integrantes:** Elena Ves Losada, Candela Francavilla y Natalia Besuschio.

Kernel monolítico x86-64 basado en x64BareBones (TP de Arquitectura de Computadoras), extendido con administración de memoria, procesos, scheduling preventivo, semáforos, pipes y shell de usuario.

## Decisiones de diseño

### Administración de memoria
Se implementaron dos administradores intercambiables mediante la interfaz `initializeMemoryManager`, `allocateMemory`, `freeMemory` y `getMemoryInfo`:

- **Free List:** First Fit, lista ordenada y fusión de bloques contiguos.
- **Buddy System:** bloques de potencias de dos, división y fusión de buddies.

El administrador se selecciona en compilación. No funcionan simultáneamente. La región disponible se obtiene del mapa de memoria física, reservando las áreas ocupadas por el kernel y los módulos.

### Procesos y planificación
Cada proceso cuenta con PCB, PID, estado, prioridad, stack propio, registros guardados, relaciones padre-hijo y descriptores de E/S. Se admiten hasta 64 procesos.

El sistema utiliza multitasking preventivo con interrupciones del timer y Round Robin con prioridades. El scheduler selecciona procesos READY, mientras que el context switch guarda y restaura los registros.

Se implementaron creación, finalización, espera de hijos, bloqueo, desbloqueo, yield, kill y modificación de prioridades.

### Sincronización
Se implementaron semáforos con nombre, identificables por procesos no relacionados, con operaciones create, open, close, wait y post.

Las estructuras compartidas se protegen mediante secciones críticas y operaciones atómicas. Los procesos que esperan recursos son bloqueados por el scheduler.

### Pipes y descriptores
Los pipes son unidireccionales, tienen buffers y extremos de lectura y escritura, y permiten comunicación entre procesos no relacionados mediante nombres.

Las operaciones de lectura y escritura son bloqueantes. Cuando no hay escritores, la lectura devuelve EOF. Cuando no hay lectores, la escritura deja de aceptar datos.

Los procesos usan descriptores STDIN y STDOUT, permitiendo redirigir su entrada y salida sin cambiar el código del programa.

### Shell y MVar
La shell permite ejecutar programas en foreground y background, crear pipelines entre dos procesos, esperar hijos y administrar procesos.

MVar utiliza una variable compartida de una posición y dos semáforos para coordinar productores y consumidores. Los escritores generan letras identificables y los lectores las consumen e imprimen. El comando principal termina después de crear los trabajadores, y un monitor realiza la limpieza al finalizar estos.

## Compilación y ejecución

La compilación debe realizarse dentro de la imagen obligatoria de la cátedra:

`agodio/itba-so-multiarch:3.1`

Desde la raíz del repositorio:

    make docker

Dentro del contenedor, compilar con Free List:

    make clean
    make MEMORY_MANAGER=freelist all

Para compilar con Buddy:

    make clean
    make MEMORY_MANAGER=buddy all

Es necesario limpiar entre configuraciones para evitar reutilizar objetos compilados con otro administrador.

Salir del contenedor mediante `exit` y ejecutar desde el host:

    ./run.sh

Se requiere Docker para compilar y QEMU para ejecutar.

### Tests auxiliares

Dentro del entorno de compilación:

    bash tests/run_tests.sh

La suite incluye pruebas auxiliares de memoria, procesos, scheduler, operaciones atómicas, semáforos, teclado, pipes, parser, programas de shell y MVar.

Estas pruebas son complementarias y no reemplazan la ejecución del kernel en QEMU.

## Instrucciones de replicación

### Comandos de la shell

| Comando | Descripción |
|---|---|
| `help` | Muestra los comandos y tests disponibles |
| `clear` | Limpia la pantalla |
| `fontup`, `fontdown` | Modifican el tamaño del texto |
| `mem` | Muestra memoria total, utilizada y libre |
| `ps` | Lista procesos, PID, estado, prioridad, FG/BG y registros |
| `loop` | Imprime periódicamente su PID |
| `kill PID` | Termina el proceso indicado |
| `nice PID PRIORIDAD` | Modifica la prioridad entre 1 y 5 |
| `block PID` | Alterna entre bloquear y desbloquear un proceso |
| `unblock PID` | Desbloquea un proceso |
| `jobs` | Muestra trabajos en background |
| `echo TEXTO` | Imprime argumentos |
| `cat` | Copia STDIN a STDOUT hasta EOF |
| `wc` | Cuenta líneas recibidas |
| `filter` | Elimina vocales |
| `mvar E L` | Ejecuta MVar con E escritores y L lectores (1 a 8) |
| `regs` | Muestra registros guardados |
| `ticks`, `time` | Muestran ticks y hora |
| `beep`, `drawdemo` | Prueban sonido y gráficos |
| `pacman` | Ejecuta la aplicación gráfica |
| `sethash N`, `gethash`, `crack` | Funciones heredadas de Arquitectura |
| `divzero`, `invalidop` | Prueban los handlers de excepciones |

### Tests provistos por la cátedra

| Test | Parámetros |
|---|---|
| `test_mm MAX_BYTES` | Reserva y libera bloques aleatorios hasta un máximo de bytes. Es continuo y comprueba integridad y solapamientos |
| `test_proc N` | Crea, bloquea, desbloquea y mata procesos aleatoriamente. N entre 1 y 16. Es continuo |
| `test_prio OBJETIVO` | Ejecuta tres trabajadores con prioridades iguales y diferentes. OBJETIVO es positivo, hasta 100000000 |
| `test_sync P I S` | Ejecuta P pares (1 a 8), I iteraciones (1 a 50000), con semáforos S=1 o sin ellos S=0 |
| `test_sync` | Sin argumentos realiza pruebas internas de semáforos y MVar |

Los tests se implementan como programas de usuario, no como built-ins de la shell.

### Caracteres especiales y atajos

- `comando &`: ejecuta en background.
- `comando1 | comando2`: conecta STDOUT del primero con STDIN del segundo.
- `Ctrl+C`: termina los procesos en foreground sin terminar la shell.
- `Ctrl+D`: envía EOF a un proceso que lee STDIN.
- `Ctrl+P` o `F6`: permite ingresar `|` cuando el teclado dificulta escribirlo.
- `Shift+-`: permite ingresar `_`.

### Ejemplos de funcionamiento

**Pipes y E/S:**

    echo hola | cat
    echo hola | wc
    echo hola | filter
    echo hola | cat &

Se esperan, respectivamente, `hola`, `1` y `hl` para los tres primeros ejemplos.

**Procesos, prioridades y control:**

    loop &
    ps
    nice PID 5
    block PID
    unblock PID
    kill PID
    jobs

Reemplazar PID por el identificador de loop obtenido con `ps`.

**Memoria:**

    mem
    test_mm 65536

Interrumpir el test continuo con Ctrl+C.

**Sincronización:**

    test_sync 2 500 1
    test_sync 2 500 0

Con semáforos, el contador final debe ser 0; sin semáforos puede variar debido a condiciones de carrera deliberadas.

**MVar:**

    mvar 2 2
    ps

Los escritores generan letras A y B. Los lectores consumen los valores de forma sincronizada. Se puede modificar la prioridad o terminar escritores mediante `nice` y `kill` para observar los cambios.

**Foreground y background:**

    test_mm 65536 &
    test_proc 4 &
    ps
    jobs

Los tests continuos ejecutados en background se terminan usando `kill PID`.

## Limitaciones conocidas

- Solo se admite un pipe entre dos procesos por pipeline.
- El buffer de entrada de la shell tiene un máximo de 64 bytes.
- La cantidad de procesos y trabajos simultáneos está limitada.
- MVar admite hasta 8 lectores, 8 escritores y 4 sesiones.
- La salida concurrente de MVar puede intercalarse con el prompt de la shell.
- El monitor de MVar consulta periódicamente la tabla de procesos y realiza yield entre consultas.
- `test_mm` y `test_proc` son tests continuos y deben interrumpirse manualmente.

## Problemas encontrados y soluciones

- **Memoria:** selección de región física segura y coexistencia de las dos implementaciones bajo una interfaz común.
- **Procesos:** manejo de stacks, relaciones padre-hijo, wait y liberación diferida de recursos.
- **Scheduling:** integración del cambio de contexto con el timer y las prioridades.
- **Sincronización:** implementación de colas de espera y bloqueo para evitar espera activa en las operaciones de semáforos.
- **Pipes:** tratamiento del EOF, cierre de extremos y limpieza de descriptores al terminar procesos.
- **Ctrl+C:** finalización de procesos foreground sin destruir la shell ni liberar memoria directamente desde la interrupción.
- **Tests:** separación de sesiones de test_sync para evitar interferencias entre ejecuciones concurrentes.
- **MVar:** creación de lectores/escritores identificables y limpieza posterior de recursos.

## Análisis estático con PVS-Studio

Se realizó análisis estático en ambas configuraciones de memoria utilizando PVS-Studio dentro de la imagen de compilación.

| Análisis | Free List | Buddy |
|---|---:|---:|
| Inicial | 28 | — |
| Final v5 | 14 | 14 |

Los reportes finales presentan el mismo conjunto de advertencias: 2 de nivel 1, 4 de nivel 2 y 8 de nivel 3.

Se corrigieron advertencias en ModulePacker, teclado, pipes y BMFS, incluyendo comprobaciones de punteros, lectura de archivos, formatos y posibles desbordamientos.

Las 14 advertencias restantes se evaluaron manualmente:

- `V1032` (BMFS, dos casos): conversiones entre arreglos de bytes y estructuras de directorio. Se agregó alineación explícita, aunque permanece la dependencia del layout de plataforma.
- `V575` y `V1004` (BMFS): comprobaciones de recursos mediante el estado de la operación, cuya relación no es seguida completamente por el analizador.
- `V522` (interrupts.c): dirección física 0 utilizada deliberadamente para la IDT.
- `V522` (processManager.c): acceso a argv condicionado por argc y las comprobaciones previas.
- `V566` (kernel.c, dos casos): direcciones físicas fijas de módulos.
- `V1009` (naiveConsole.c): buffer completado antes de imprimirse.
- `V566` (naiveConsole.c, dos casos): dirección de memoria de video VGA.
- `V1044` (pipeSync.c): la extracción de la cola decrementa indirectamente el contador.
- `V525` (pacman.c, dos casos): tamaños de rectángulos deliberados para dibujar la boca.

Estas explicaciones corresponden a una revisión manual. No se afirma que PVS-Studio haya emitido cero advertencias. Los casos de BMFS conservan dependencias de plataforma que deben tenerse en cuenta.

## Uso de IA y atribuciones

Se utilizó ChatGPT como asistencia durante el desarrollo para analizar decisiones de diseño, propuestas de implementación, fallos de compilación, resultados de pruebas y documentación.

La asistencia incluyó revisión y propuestas relacionadas con procesos, sincronización, tests e implementación de MVar, entre otros componentes. Entre las funciones y archivos revisados con asistencia se encuentran `testSyncPairs.c`, `mvarDemo.c`, el administrador de procesos y los mecanismos de sincronización.

Las propuestas incorporadas fueron revisadas y adaptadas para integrarlas con la arquitectura del proyecto. El código de base proviene de x64BareBones y del trabajo desarrollado por el grupo.

## Créditos

Proyecto basado en x64BareBones. Se conserva `License.txt`, con los créditos y las condiciones de licencia originales.

Autores mencionados en los materiales originales: Rodrigo Rearden (RowDaBoat) y Augusto Nizzo McIntosh.
