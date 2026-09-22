# Manual de Usuario

## Trabajo Práctico Especial - Arquitectura de Computadoras

Este sistema operativo bootea mediante Pure64 y ejecuta un kernel propio en modo gráfico. Al iniciar, se muestra una shell interactiva con un prompt desde donde se pueden ejecutar los comandos implementados.

## Compilación

Para compilar el proyecto desde cero, ingresar al entorno de compilación y ejecutar:

```bash
make clean
make all
```

Luego, para ejecutar el sistema en QEMU:

```bash
./run.sh
```

Al iniciar correctamente, el sistema muestra el prompt:

```text
>
```

Desde ese momento se pueden ingresar comandos por teclado.

## Comandos disponibles

### `help`

Muestra la lista de comandos disponibles.

Uso:

```text
help
```

### `clear`

Limpia la pantalla de la terminal y vuelve a mostrar el prompt.

Uso:

```text
clear
```

### `fontup`

Aumenta el tamaño del texto de la shell. El tamaño tiene un máximo para mantener la terminal usable.

Uso:

```text
fontup
```

### `fontdown`

Reduce el tamaño del texto de la shell.

Uso:

```text
fontdown
```

### `time`

Muestra la hora actual obtenida desde el RTC/CMOS.

Uso:

```text
time
```

Ejemplo de salida:

```text
20:26:06
```

La hora mostrada puede depender de la configuración del RTC expuesto por QEMU.

### `ticks`

Muestra la cantidad de ticks del timer desde el inicio del sistema.

Uso:

```text
ticks
```

Este comando se utiliza como diagnóstico para verificar el funcionamiento del timer IRQ0 y de la syscall asociada.

### `beep`

Emite un sonido corto mediante el PC speaker.

Uso:

```text
beep
```

### `drawdemo`

Ejecuta una demostración gráfica simple dibujando rectángulos en pantalla.

Uso:

```text
drawdemo
```

Este comando sirve como diagnóstico para verificar la syscall gráfica usada luego por Pacman.

### `regs`

Muestra el último snapshot de registros guardado por el kernel.

Uso:

```text
regs
```

Para guardar un snapshot, presionar la tecla `ESC` y luego ejecutar:

```text
regs
```

Se muestran registros generales del procesador junto con RIP, CS y RFLAGS.

### `divzero`

Genera una excepción de división por cero para verificar el handler correspondiente.

Uso:

```text
divzero
```

El sistema muestra el tipo de excepción, RIP y los registros del procesador, y luego vuelve a la shell.

### `invalidop`

Genera una excepción de código de operación inválido usando una instrucción inválida.

Uso:

```text
invalidop
```

El sistema muestra el tipo de excepción, RIP y los registros del procesador, y luego vuelve a la shell.

## Pacman

### `pacman`

Ejecuta el juego Pacman en modo gráfico.

Uso:

```text
pacman
```

Al iniciar, se muestra un menú con las opciones:

```text
1 - Un jugador
2 - Dos jugadores
q - Volver a la shell
```

### Modo 1 jugador

En modo 1 jugador, el usuario controla a Pacman (el cuadrado azul) y el fantasma(el cuadrado rojo) se mueve automáticamente mediante una IA simple.

Controles:

```text
W - mover arriba
A - mover izquierda
S - mover abajo
D - mover derecha
q - salir del juego
```

Objetivo:

* Comer pellets para sumar puntos.
* Evitar que el fantasma alcance a Pacman.
* Si Pacman colisiona con el fantasma, aparece `GAME OVER`.
* Si se comen todos los pellets, aparece `YOU WIN`.

### Modo 2 jugadores

En modo 2 jugadores, un jugador controla a Pacman y otro controla al fantasma.

Controles de Pacman:

```text
W - mover arriba
A - mover izquierda
S - mover abajo
D - mover derecha
```

Controles del fantasma:

```text
I - mover arriba
J - mover izquierda
K - mover abajo
L - mover derecha
```

Comando para salir:

```text
q
```

Durante el juego se mantiene un puntaje en pantalla y se emite sonido al comer pellets.

## Kernel Cracker

El Kernel Cracker busca por fuerza bruta una password de cinco letras minúsculas que genere un hash objetivo usando la función hash indicada por la cátedra.

El espacio de usuario no realiza el crackeo directamente. La shell invoca syscalls y el kernel realiza la búsqueda.

### `sethash N`

Guarda en kernel el hash objetivo.

Uso:

```text
sethash 210706178890
```

Si el formato es inválido, se muestra:

```text
Uso: sethash <numero>
```

### `gethash`

Muestra el hash objetivo actualmente guardado en kernel.

Uso:

```text
gethash
```

Ejemplo:

```text
Hash actual: 210706178890
```

### `crack`

Ejecuta el Kernel Cracker.

Uso:

```text
crack
```

Ejemplo:

```text
sethash 210706178890
crack
```

Salida esperada:

```text
Password encontrada: aaaaa
Ciclos: ...
Tiempo aprox: ...
```

Ejemplos útiles para la demostración:

```text
sethash 210706178890
crack
```

Resultado esperado:

```text
Password encontrada: aaaaa
```

```text
sethash 210706178891
crack
```

Resultado esperado:

```text
Password encontrada: aaaab
```

```text
sethash 210706217108
crack
```

Resultado esperado:

```text
Password encontrada: abcde
```

Si el hash no corresponde a ninguna combinación de cinco letras minúsculas entre `aaaaa` y `zzzzz`, el sistema informa que no encontró password.

## Prueba sugerida del sistema

Para verificar el funcionamiento general del sistema, ejecutar:

```text
help
clear
fontup
fontdown
time
ticks
beep
drawdemo
regs
```

Luego presionar `ESC` y ejecutar:

```text
regs
```

Después probar excepciones:

```text
divzero
invalidop
```

Probar Pacman:

```text
pacman
```

Probar Cracker:

```text
sethash 210706178890
gethash
crack
```

Al finalizar cada módulo, el sistema debe volver correctamente al prompt de la shell.
