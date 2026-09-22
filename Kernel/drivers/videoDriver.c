#include <videoDriver.h>
#include <modernDos.h>

struct vbe_mode_info_structure {
	uint16_t attributes;		// deprecated, only bit 7 should be of interest to you, and it indicates the mode supports a linear frame buffer.
	uint8_t window_a;			// deprecated
	uint8_t window_b;			// deprecated
	uint16_t granularity;		// deprecated; used while calculating bank numbers
	uint16_t window_size;
	uint16_t segment_a;
	uint16_t segment_b;
	uint32_t win_func_ptr;		// deprecated; used to switch banks from protected mode without returning to real mode
	uint16_t pitch;			// number of bytes per horizontal line
	uint16_t width;			// width in pixels
	uint16_t height;			// height in pixels
	uint8_t w_char;			// unused...
	uint8_t y_char;			// ...
	uint8_t planes;
	uint8_t bpp;			// bits per pixel in this mode
	uint8_t banks;			// deprecated; total number of banks in this mode
	uint8_t memory_model;
	uint8_t bank_size;		// deprecated; size of a bank, almost always 64 KB but may be 16 KB...
	uint8_t image_pages;
	uint8_t reserved0;
 
	uint8_t red_mask;
	uint8_t red_position;
	uint8_t green_mask;
	uint8_t green_position;
	uint8_t blue_mask;
	uint8_t blue_position;
	uint8_t reserved_mask;
	uint8_t reserved_position;
	uint8_t direct_color_attributes;
 
	uint32_t framebuffer;		// physical address of the linear frame buffer; write here to draw to the screen
	uint32_t off_screen_mem_off;
	uint16_t off_screen_mem_size;	// size of memory in the framebuffer but not being displayed on the screen
	uint8_t reserved1[206];
} __attribute__ ((packed));

typedef struct vbe_mode_info_structure * VBEInfoPtr;

VBEInfoPtr VBE_mode_info = (VBEInfoPtr) 0x0000000000005C00;

void putPixel(uint32_t hexColor, uint64_t x, uint64_t y) {
    uint8_t * framebuffer = (uint8_t *) (uintptr_t) VBE_mode_info->framebuffer;
    uint64_t offset = (x * ((VBE_mode_info->bpp)/8)) + (y * VBE_mode_info->pitch);
    framebuffer[offset]     =  (hexColor) & 0xFF;
    framebuffer[offset+1]   =  (hexColor >> 8) & 0xFF; 
    framebuffer[offset+2]   =  (hexColor >> 16) & 0xFF;
}

void drawRect(uint32_t hexColor, uint64_t x, uint64_t y, uint64_t width, uint64_t height) {
    for (uint64_t row = 0; row < height; row++) {
        for (uint64_t col = 0; col < width; col++) {
            putPixel(hexColor, x + col, y + row);
        }
    }
}

void clearScreen(uint32_t hexColor) {
    drawRect(hexColor, 0, 0, getScreenWidth(), getScreenHeight());
}

uint16_t getScreenWidth() {
    return VBE_mode_info->width;
}

uint16_t getScreenHeight() {
    return VBE_mode_info->height;
}

void drawChar(char c, uint64_t x, uint64_t y, uint32_t fgColor, uint32_t bgColor, uint8_t scale) {
    unsigned char uc = (unsigned char)c;

    for (uint64_t row = 0; row < modernDos_inf.height; row++) {
        uint8_t bitmapRow = modernDos_bmp[uc][row];

        for (uint64_t col = 0; col < modernDos_inf.width; col++) {
            uint32_t color = (bitmapRow & (1 << col)) ? fgColor : bgColor;

            for (uint8_t dy = 0; dy < scale; dy++) {
                for (uint8_t dx = 0; dx < scale; dx++) {
                    putPixel(color, x + col * scale + dx, y + row * scale + dy);
                }
            }
        }
    }
}

void drawString(const char *str, uint64_t x, uint64_t y, uint32_t fgColor, uint32_t bgColor, uint8_t scale) {
    uint64_t cursorX = x;
    uint64_t cursorY = y;

    for (uint64_t i = 0; str[i] != 0; i++) {
        if (str[i] == '\n') {
            cursorX = x;
            cursorY += modernDos_inf.height * scale;
        } else {
            drawChar(str[i], cursorX, cursorY, fgColor, bgColor, scale);
            cursorX += modernDos_inf.width * scale;
        }
    }
}