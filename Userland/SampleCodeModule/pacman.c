#include <stdint.h>
#include <syscalls.h>
#include <pacman.h>

#define CELL_SIZE 40
#define MAP_ROWS 15
#define MAP_COLS 21

#define OFFSET_X 92
#define OFFSET_Y 120

#define COLOR_BLACK  0x00000000
#define COLOR_BLUE   0x000000FF
#define COLOR_YELLOW 0x00FFFF00
#define COLOR_RED    0x00FF0000
#define COLOR_WHITE  0x00FFFFFF
#define COLOR_DOT    0x00AAAAAA

#define SCORE_DIGIT_WIDTH  8
#define SCORE_DIGIT_HEIGHT 14
#define SCORE_DIGIT_GAP    3

#define SCORE_X 85
#define SCORE_Y 36

#define PLAYER_ONE 1
#define PLAYER_TWO 2

#define DIR_RIGHT 0
#define DIR_LEFT  1
#define DIR_UP    2
#define DIR_DOWN  3

/*
 * Mapa base del juego.
 *
 * Se mantiene constante para poder reiniciar la partida
 * cada vez que se ejecuta el comando pacman.
 */
static const char baseMap[MAP_ROWS][MAP_COLS + 1] = {
    "#####################",
    "#.........#.........#",
    "#.###.###.#.###.###.#",
    "#...................#",
    "#.###.#.#####.#.###.#",
    "#.....#...#...#.....#",
    "#####.### # ###.#####",
    "    #.#       #.#    ",
    "#####.# ##### #.#####",
    "#.........#.........#",
    "#.###.###.#.###.###.#",
    "#...#...........#...#",
    "###.#.#.#####.#.#.###",
    "#.....#...#...#.....#",
    "#####################"
};

/*
 * Mapa activo de la partida.
 *
 * Este mapa sí se modifica cuando Pacman come pellets.
 */
static char map[MAP_ROWS][MAP_COLS + 1];

/*
 * Cantidad de pellets que quedan por comer.
 * Se usa para detectar la condicion de victoria.
 */
static uint64_t remainingPellets = 0;

/*
 * Estado general de la partida.
 */
static uint64_t score = 0;
static uint8_t gameMode = PLAYER_ONE;

/*
 * Posicion actual de Pacman.
 */
static int pacmanRow = 1;
static int pacmanCol = 1;
static uint8_t pacmanDirection = DIR_RIGHT;

/*
 * Posicion actual del fantasma.
 */
static int ghostRow = 13;
static int ghostCol = 19;

/*
 * Calcula longitud de string.
 */
static uint64_t stringLength(const char *str) {
    uint64_t length = 0;

    while (str[length] != 0) {
        length++;
    }

    return length;
}

/*
 * Copia una cadena terminada en cero.
 *
 * Se usa para reiniciar el mapa activo desde el mapa base.
 */
static void stringCopy(char *destination, const char *source) {
    uint64_t i = 0;

    while (source[i] != 0) {
        destination[i] = source[i];
        i++;
    }

    destination[i] = 0;
}

/*
 * Imprime texto usando stdout.
 */
static void print(const char *str) {
    sysWrite(STDOUT, str, stringLength(str));
}

/*
 * Imprime un numero decimal sin signo.
 */
static void printDecimal(uint64_t value) {
    char buffer[21];
    uint64_t index = 0;

    if (value == 0) {
        print("0");
        return;
    }

    while (value > 0 && index < sizeof(buffer)) {
        buffer[index++] = '0' + (value % 10);
        value /= 10;
    }

    while (index > 0) {
        index--;
        sysWrite(STDOUT, &buffer[index], 1);
    }
}

/*
 * Dibuja una celda del tablero.
 */
static void drawCell(uint64_t row, uint64_t col, uint32_t color) {
    sysDrawRect(
        color,
        OFFSET_X + col * CELL_SIZE,
        OFFSET_Y + row * CELL_SIZE,
        CELL_SIZE,
        CELL_SIZE
    );
}

/*
 * Dibuja un segmento de un digito estilo display digital.
 *
 * Se usa para mostrar el puntaje dentro del juego sin depender
 * de la terminal textual ni redibujar toda la pantalla.
 */
static void drawSegment(uint64_t x, uint64_t y, uint64_t width, uint64_t height) {
    sysDrawRect(COLOR_WHITE, x, y, width, height);
}

/*
 * Limpia el area donde se muestra el puntaje.
 *
 * Esto permite actualizar el score sin limpiar toda la pantalla
 * y sin generar flicker.
 */
static void clearScoreArea(void) {
    sysDrawRect(COLOR_BLACK, SCORE_X, SCORE_Y, 120, 20);
}

/*
 * Dibuja un digito decimal usando segmentos.
 *
 * Cada bit de mask representa un segmento:
 * 0 = arriba
 * 1 = arriba derecha
 * 2 = abajo derecha
 * 3 = abajo
 * 4 = abajo izquierda
 * 5 = arriba izquierda
 * 6 = medio
 */
static void drawDigit(uint8_t digit, uint64_t x, uint64_t y) {
    static const uint8_t masks[10] = {
        0x3F, /* 0 */
        0x06, /* 1 */
        0x5B, /* 2 */
        0x4F, /* 3 */
        0x66, /* 4 */
        0x6D, /* 5 */
        0x7D, /* 6 */
        0x07, /* 7 */
        0x7F, /* 8 */
        0x6F  /* 9 */
    };

    uint8_t mask = masks[digit];
    uint64_t thickness = 2;
    uint64_t w = SCORE_DIGIT_WIDTH;
    uint64_t h = SCORE_DIGIT_HEIGHT;

    if (mask & 0x01) {
        drawSegment(x, y, w, thickness);
    }

    if (mask & 0x02) {
        drawSegment(x + w - thickness, y, thickness, h / 2);
    }

    if (mask & 0x04) {
        drawSegment(x + w - thickness, y + h / 2, thickness, h / 2);
    }

    if (mask & 0x08) {
        drawSegment(x, y + h - thickness, w, thickness);
    }

    if (mask & 0x10) {
        drawSegment(x, y + h / 2, thickness, h / 2);
    }

    if (mask & 0x20) {
        drawSegment(x, y, thickness, h / 2);
    }

    if (mask & 0x40) {
        drawSegment(x, y + h / 2 - thickness / 2, w, thickness);
    }
}

/*
 * Dibuja el puntaje actual en pantalla.
 *
 * El score se dibuja con rectangulos, no con texto de terminal,
 * para poder actualizarlo sin mover el cursor de la shell.
 */
static void drawScore(void) {
    uint64_t value = score;
    uint8_t digits[10];
    uint64_t count = 0;

    clearScoreArea();

    if (value == 0) {
        drawDigit(0, SCORE_X, SCORE_Y);
        return;
    }

    while (value > 0 && count < 10) {
        digits[count++] = value % 10;
        value /= 10;
    }

    for (uint64_t i = 0; i < count; i++) {
        drawDigit(
            digits[count - 1 - i],
            SCORE_X + i * (SCORE_DIGIT_WIDTH + SCORE_DIGIT_GAP),
            SCORE_Y
        );
    }
}

/*
 * Dibuja un pellet chico dentro de una celda.
 */
static void drawPellet(uint64_t row, uint64_t col) {
    uint64_t size = CELL_SIZE / 5;

    sysDrawRect(
        COLOR_DOT,
        OFFSET_X + col * CELL_SIZE + (CELL_SIZE / 2) - (size / 2),
        OFFSET_Y + row * CELL_SIZE + (CELL_SIZE / 2) - (size / 2),
        size,
        size
    );
}

/*
 * Redibuja una celda individual del mapa.
 *
 * Se usa para evitar redibujar toda la pantalla en cada movimiento.
 * Esto reduce el parpadeo porque solo se actualizan las celdas
 * afectadas por Pacman o el fantasma.
 */
static void drawMapCell(uint64_t row, uint64_t col) {
    if (map[row][col] == '#') {
        drawCell(row, col, COLOR_BLUE);
        return;
    }

    drawCell(row, col, COLOR_BLACK);

    if (map[row][col] == '.') {
        drawPellet(row, col);
    }
}

/*
 * Dibuja el tablero completo.
 *
 * Solo se llama al iniciar una partida, no en cada frame.
 */
static void drawMap(void) {
    for (uint64_t row = 0; row < MAP_ROWS; row++) {
        for (uint64_t col = 0; col < MAP_COLS; col++) {
            drawMapCell(row, col);
        }
    }
}

/*
 * Dibuja a Pacman centrado dentro de su celda.
 */
static void drawPacman(void) {
    uint64_t x = OFFSET_X + pacmanCol * CELL_SIZE;
    uint64_t y = OFFSET_Y + pacmanRow * CELL_SIZE;

    uint64_t margin = 3;
    uint64_t size = CELL_SIZE - 2 * margin;

    sysDrawRect(COLOR_YELLOW, x + margin + 2, y + margin,     size - 4, size);
    sysDrawRect(COLOR_YELLOW, x + margin,     y + margin + 3, size,     size - 6);

    switch (pacmanDirection) {
        case DIR_RIGHT:
            sysDrawRect(COLOR_BLACK, x + margin + size - 10, y + margin + 4, 10, 4);
            sysDrawRect(COLOR_BLACK, x + margin + size - 12, y + margin + 8, 12, 4);
            sysDrawRect(COLOR_BLACK, x + margin + size - 10, y + margin + 12, 10, 4);
            break;

        case DIR_LEFT:
            sysDrawRect(COLOR_BLACK, x + margin, y + margin + 4, 10, 4);
            sysDrawRect(COLOR_BLACK, x + margin, y + margin + 8, 12, 4);
            sysDrawRect(COLOR_BLACK, x + margin, y + margin + 12, 10, 4);
            break;

        case DIR_UP:
            sysDrawRect(COLOR_BLACK, x + margin + 4,  y + margin, 4, 10);
            sysDrawRect(COLOR_BLACK, x + margin + 8,  y + margin, 4, 12);
            sysDrawRect(COLOR_BLACK, x + margin + 12, y + margin, 4, 10);
            break;

        case DIR_DOWN:
            sysDrawRect(COLOR_BLACK, x + margin + 4,  y + margin + size - 10, 4, 10);
            sysDrawRect(COLOR_BLACK, x + margin + 8,  y + margin + size - 12, 4, 12);
            sysDrawRect(COLOR_BLACK, x + margin + 12, y + margin + size - 10, 4, 10);
            break;
    }
}

/*
 * Dibuja al fantasma centrado dentro de su celda.
 */
static void drawGhost(void) {
    uint64_t x = OFFSET_X + ghostCol * CELL_SIZE;
    uint64_t y = OFFSET_Y + ghostRow * CELL_SIZE;

    uint64_t margin = 3;
    uint64_t size = CELL_SIZE - 2 * margin;

    /*
     * Cuerpo rojo centrado.
     */
    sysDrawRect(COLOR_RED, x + margin + 2, y + margin,     size - 4, size - 3);
    sysDrawRect(COLOR_RED, x + margin,     y + margin + 5, size,     size - 6);

    /*
     * Base irregular del fantasma.
     */
    sysDrawRect(COLOR_BLACK, x + margin + 2,  y + margin + size - 3, 3, 3);
    sysDrawRect(COLOR_RED,   x + margin + 5,  y + margin + size - 3, 3, 3);
    sysDrawRect(COLOR_BLACK, x + margin + 8,  y + margin + size - 3, 3, 3);
    sysDrawRect(COLOR_RED,   x + margin + 11, y + margin + size - 3, 3, 3);
    sysDrawRect(COLOR_BLACK, x + margin + 14, y + margin + size - 3, 3, 3);

    /*
     * Ojos.
     */
    sysDrawRect(COLOR_WHITE, x + margin + 4,  y + margin + 5, 4, 5);
    sysDrawRect(COLOR_WHITE, x + margin + 10, y + margin + 5, 4, 5);

    /*
     * Pupilas.
     */
    sysDrawRect(COLOR_BLACK, x + margin + 6,  y + margin + 7, 2, 2);
    sysDrawRect(COLOR_BLACK, x + margin + 12, y + margin + 7, 2, 2);
}

/*
 * Dibuja la pantalla inicial del juego.
 */
static void renderInitialGame(void) {
    sysClear();

    /*
     * No usamos texto de terminal dentro del juego para que
     * Pacman no dependa de la escala configurada en la shell.
     * El tablero, actores y puntaje se dibujan graficamente.
     */
    drawMap();
    drawScore();
    drawPacman();
    drawGhost();
}

/*
 * Redibuja los actores sobre el mapa.
 *
 * Se utiliza luego de mover Pacman o el fantasma para
 * restaurar su representacion grafica.
 */
static void drawActors(void) {
    drawPacman();
    drawGhost();
}

/*
 * Indica si una posicion del mapa es transitable.
 */
static uint8_t canMoveTo(int row, int col) {
    if (row < 0 || row >= MAP_ROWS || col < 0 || col >= MAP_COLS) {
        return 0;
    }

    return map[row][col] != '#';
}

/*
 * Mueve Pacman si la nueva posicion es valida.
 *
 * Solo redibuja las celdas afectadas para evitar flicker.
 * Si come un pellet, suma puntos y emite sonido.
 */
static void movePacman(int deltaRow, int deltaCol) {
    int oldRow = pacmanRow;
    int oldCol = pacmanCol;

    int newRow = pacmanRow + deltaRow;
    int newCol = pacmanCol + deltaCol;

    if (!canMoveTo(newRow, newCol)) {
        return;
    }

    pacmanRow = newRow;
    pacmanCol = newCol;

    if (map[pacmanRow][pacmanCol] == '.') {
        map[pacmanRow][pacmanCol] = ' ';
        score += 10;

        if (remainingPellets > 0) {
            remainingPellets--;
        }

        drawScore();
        sysBeep(440, 4);
    }

    drawMapCell(oldRow, oldCol);
    drawMapCell(pacmanRow, pacmanCol);
    drawActors();
}

/*
 * Mueve el fantasma a una posicion valida.
 */
static void moveGhost(int deltaRow, int deltaCol) {
    int oldRow = ghostRow;
    int oldCol = ghostCol;

    int newRow = ghostRow + deltaRow;
    int newCol = ghostCol + deltaCol;

    if (!canMoveTo(newRow, newCol)) {
        return;
    }

    ghostRow = newRow;
    ghostCol = newCol;

    drawMapCell(oldRow, oldCol);
    drawMapCell(ghostRow, ghostCol);
    drawActors();
}

/*
 * IA simple para modo 1 jugador.
 *
 * El fantasma intenta acercarse a Pacman. Si el camino principal
 * esta bloqueado por una pared, prueba direcciones alternativas
 * para no quedarse quieto.
 */
static void moveGhostAI(void) {
    int deltaRow = pacmanRow - ghostRow;
    int deltaCol = pacmanCol - ghostCol;

    if (deltaCol < 0 && canMoveTo(ghostRow, ghostCol - 1)) {
        moveGhost(0, -1);
        return;
    }

    if (deltaCol > 0 && canMoveTo(ghostRow, ghostCol + 1)) {
        moveGhost(0, 1);
        return;
    }

    if (deltaRow < 0 && canMoveTo(ghostRow - 1, ghostCol)) {
        moveGhost(-1, 0);
        return;
    }

    if (deltaRow > 0 && canMoveTo(ghostRow + 1, ghostCol)) {
        moveGhost(1, 0);
        return;
    }

    /*
     * Fallback: si no pudo acercarse directamente,
     * prueba cualquier movimiento valido.
     */
    if (canMoveTo(ghostRow, ghostCol - 1)) {
        moveGhost(0, -1);
        return;
    }

    if (canMoveTo(ghostRow, ghostCol + 1)) {
        moveGhost(0, 1);
        return;
    }

    if (canMoveTo(ghostRow - 1, ghostCol)) {
        moveGhost(-1, 0);
        return;
    }

    if (canMoveTo(ghostRow + 1, ghostCol)) {
        moveGhost(1, 0);
        return;
    }
}

/*
 * Verifica si Pacman gano la partida.
 *
 * La condicion de victoria simplificada es haber comido
 * todos los pellets del mapa.
 */
static uint8_t hasWon(void) {
    return remainingPellets == 0;
}

/*
 * Procesa una tecla durante el juego.
 *
 * WASD mueve Pacman.
 * IJKL mueve el fantasma en modo 2 jugadores.
 * q sale del juego.
 */
static uint8_t processInput(char key) {
    switch (key) {
        case 'w':
            pacmanDirection = DIR_UP;
            movePacman(-1, 0);
            break;

        case 's':
            pacmanDirection = DIR_DOWN;
            movePacman(1, 0);
            break;

        case 'a':
            pacmanDirection = DIR_LEFT;
            movePacman(0, -1);
            break;

        case 'd':
            pacmanDirection = DIR_RIGHT;
            movePacman(0, 1);
            break;

        case 'i':
            if (gameMode == PLAYER_TWO) {
                moveGhost(-1, 0);
            }
            break;

        case 'k':
            if (gameMode == PLAYER_TWO) {
                moveGhost(1, 0);
            }
            break;

        case 'j':
            if (gameMode == PLAYER_TWO) {
                moveGhost(0, -1);
            }
            break;

        case 'l':
            if (gameMode == PLAYER_TWO) {
                moveGhost(0, 1);
            }
            break;

        case 'q':
            return 0;
    }

    return 1;
}

/*
 * Muestra el menu inicial del juego y espera seleccion.
 *
 * Retorna:
 *   1 si se eligio un modo de juego valido.
 *   0 si el usuario cancelo y quiere volver a la shell.
 */
static uint8_t selectGameMode(void) {
    char key;

    sysClear();

    print("PACMAN\n\n");
    print("1 - Un jugador: Pacman vs fantasma IA\n");
    print("2 - Dos jugadores: Pacman vs fantasma humano\n");
    print("q - Volver a la shell\n\n");

    print("Controles:\n");
    print("  Pacman: WASD\n");
    print("  Fantasma jugador 2: IJKL\n");
    print("  Salir durante el juego: q\n\n");

    print("Selecciona modo: ");

    while (1) {
        if (sysRead(STDIN, &key, 1) == 0) {
            continue;
        }

        if (key == '1') {
            gameMode = PLAYER_ONE;
            return 1;
        }

        if (key == '2') {
            gameMode = PLAYER_TWO;
            return 1;
        }

        if (key == 'q') {
            return 0;
        }
    }
}

/*
 * Reinicia posiciones, puntaje y mapa del juego.
 *
 * Copia baseMap sobre map para que cada partida empiece
 * con todos los pellets disponibles nuevamente.
 */
static void resetGame(void) {
    score = 0;
    remainingPellets = 0;

    for (uint64_t row = 0; row < MAP_ROWS; row++) {
        stringCopy(map[row], baseMap[row]);

        for (uint64_t col = 0; col < MAP_COLS; col++) {
            if (map[row][col] == '.') {
                remainingPellets++;
            }
        }
    }

    pacmanRow = 1;
    pacmanCol = 1;
    pacmanDirection = DIR_RIGHT;

    ghostRow = 13;
    ghostCol = 19;
}

void runPacman(void) {
    char key;
    uint64_t lastGhostTick;

    resetGame();

    if (!selectGameMode()) {
        sysClear();
        print("Pacman cancelado\n");
        return;
    }

    lastGhostTick = sysGetTicks();

    renderInitialGame();

    while (1) {
        /*
         * Procesamos todas las teclas pendientes.
         */
        while (sysRead(STDIN, &key, 1) == 1) {
            if (!processInput(key)) {
                sysClear();
                return;
            }

            if (hasWon()) {
                sysBeep(660, 10);
                sysClear();
                print("YOU WIN\n");
                print("Score final: ");
                printDecimal(score);
                print("\n");
                return;
            }
        }

        /*
         * En modo 1 jugador, el fantasma se mueve solo
         * cada cierta cantidad de ticks.
         */
        if (gameMode == PLAYER_ONE && sysGetTicks() - lastGhostTick >= 12) {
            moveGhostAI();
            lastGhostTick = sysGetTicks();
        }

        if (hasWon()) {
            sysBeep(660, 10);
            sysClear();
            print("YOU WIN\n");
            print("Score final: ");
            printDecimal(score);
            print("\n");
            return;
        }

        if (pacmanRow == ghostRow && pacmanCol == ghostCol) {
            sysBeep(220, 8);
            sysClear();
            print("GAME OVER\n");
            print("Score final: ");
            printDecimal(score);
            print("\n");
            return;
        }
    }
}