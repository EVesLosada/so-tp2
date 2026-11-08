#include <stdint.h>
#include <syscalls.h>

#define MM_MAX_BLOCKS 64
#define MM_MAX_BLOCK_SIZE 2048

typedef struct {
    uint8_t *ptr;
    uint64_t size;
    uint8_t pattern;
} TestMmBlock;

static void mmPrint(const char *text) {
    uint64_t length = 0;

    while (text[length] != '\0') {
        length++;
    }

    sysWrite(STDOUT, text, length);
}

static uint8_t mmParseSize(const char *text, uint64_t *value) {
    uint64_t result = 0;

    if (text == 0 || text[0] == '\0' || value == 0) {
        return 0;
    }

    for (uint64_t i = 0; text[i] != '\0'; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return 0;
        }

        uint64_t digit = (uint64_t)(text[i] - '0');

        if (result > (UINT64_MAX - digit) / 10) {
            return 0;
        }

        result = result * 10 + digit;
    }

    if (result == 0) {
        return 0;
    }

    *value = result;
    return 1;
}

static uint64_t mmRandom(uint64_t *state) {
    uint64_t value = *state;

    value ^= value << 13;
    value ^= value >> 7;
    value ^= value << 17;

    *state = value;
    return value;
}

static void mmFill(TestMmBlock *block) {
    for (uint64_t i = 0; i < block->size; i++) {
        block->ptr[i] = block->pattern;
    }
}

static uint8_t mmVerify(const TestMmBlock *block) {
    for (uint64_t i = 0; i < block->size; i++) {
        if (block->ptr[i] != block->pattern) {
            return 0;
        }
    }

    return 1;
}

static uint8_t mmOverlaps(
    const TestMmBlock *first,
    const TestMmBlock *second
) {
    uintptr_t a = (uintptr_t)first->ptr;
    uintptr_t b = (uintptr_t)second->ptr;

    if (a > UINTPTR_MAX - first->size ||
        b > UINTPTR_MAX - second->size) {
        return 1;
    }

    return a < b + second->size &&
           b < a + first->size;
}

static void mmRelease(
    TestMmBlock blocks[MM_MAX_BLOCKS]
) {
    for (uint64_t i = 0; i < MM_MAX_BLOCKS; i++) {
        if (blocks[i].ptr != 0) {
            sysFree(blocks[i].ptr);
            blocks[i].ptr = 0;
        }
    }
}

void shellTestMmProcess(uint64_t argc, char **argv) {
    uint64_t maxBytes;

    if (argc != 2 || argv == 0 ||
        !mmParseSize(argv[1], &maxBytes)) {
        mmPrint("ERROR: uso test_mm <max_bytes>\n");
        return;
    }

    TestMmBlock blocks[MM_MAX_BLOCKS];

    for (uint64_t i = 0; i < MM_MAX_BLOCKS; i++) {
        blocks[i].ptr = 0;
        blocks[i].size = 0;
        blocks[i].pattern = 0;
    }

    uint64_t usedBytes = 0;
    uint64_t activeBlocks = 0;

    uint64_t randomState =
        sysGetTicks() ^ (sysGetPid() << 17) ^
        0x9E3779B97F4A7C15ULL;

    if (randomState == 0) {
        randomState = 1;
    }

    for (;;) {
        /* Comprobar que ningun bloque haya sido alterado. */
        for (uint64_t i = 0; i < MM_MAX_BLOCKS; i++) {
            if (blocks[i].ptr != 0 && !mmVerify(&blocks[i])) {
                mmPrint("ERROR test_mm: datos corruptos\n");
                mmRelease(blocks);
                return;
            }
        }

        uint8_t shouldAllocate =
            activeBlocks == 0 ||
            (activeBlocks < MM_MAX_BLOCKS &&
             usedBytes < maxBytes &&
             mmRandom(&randomState) % 2 == 0);

        if (shouldAllocate && usedBytes < maxBytes) {
            uint64_t available = maxBytes - usedBytes;

            uint64_t size =
                1 + mmRandom(&randomState) % MM_MAX_BLOCK_SIZE;

            if (size > available) {
                size = available;
            }

            uint64_t slot = 0;

            while (slot < MM_MAX_BLOCKS &&
                   blocks[slot].ptr != 0) {
                slot++;
            }

            if (slot < MM_MAX_BLOCKS) {
                uint8_t *ptr = (uint8_t *)sysMalloc(size);

                if (ptr != 0) {
                    TestMmBlock candidate;

                    candidate.ptr = ptr;
                    candidate.size = size;
                    candidate.pattern =
                        (uint8_t)mmRandom(&randomState);

                    for (uint64_t i = 0; i < MM_MAX_BLOCKS; i++) {
                        if (blocks[i].ptr != 0 &&
                            mmOverlaps(&candidate, &blocks[i])) {
                            mmPrint(
                                "ERROR test_mm: bloques solapados\n"
                            );

                            /*
                             * No liberar el bloque conflictivo:
                             * podria compartir memoria con otro.
                             */
                            mmRelease(blocks);
                            return;
                        }
                    }

                    blocks[slot] = candidate;
                    mmFill(&blocks[slot]);

                    usedBytes += size;
                    activeBlocks++;
                }
            }
        } else if (activeBlocks > 0) {
            uint64_t target =
                mmRandom(&randomState) % activeBlocks;

            for (uint64_t i = 0; i < MM_MAX_BLOCKS; i++) {
                if (blocks[i].ptr == 0) {
                    continue;
                }

                if (target == 0) {
                    usedBytes -= blocks[i].size;
                    sysFree(blocks[i].ptr);

                    blocks[i].ptr = 0;
                    blocks[i].size = 0;
                    activeBlocks--;
                    break;
                }

                target--;
            }
        }

        sysYield();
    }
}
