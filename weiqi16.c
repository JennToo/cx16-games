#include <cbm.h>
#include <cx16.h>

#define VGA_MODE 0b00000001
#define CHROMA_DISABLE 0b00000100
#define LAYER_0_ENABLE 0b00010000
#define LAYER_1_ENABLE 0b00100000
#define SPRITES_ENABLE 0b01000000

#define VERA_ADDR_2 1

#define MAP_WIDTH_64_TILES 0b00010000
#define MAP_HEIGHT_32_TILES 0b00000000
#define LAYER_TEXT_256C 0b00001000
#define MAP_COLOR_DEPTH_1BPP 0b00000000
#define LAYER0_MAP_ADDRESS 0x0
#define LAYER1_MAP_ADDRESS 0x1000

#define TILE_BASE_ADDRESS 0x2000
#define TILE_WIDTH_8 0
#define TILE_HEIGHT_8 0

#define DEFAULT_CHARS_BANK 6
#define DEFAULT_CHARS_ADDR 0xC000

#define BROWN 9
#define WHITE 1
#define BLACK 0
#define GREEN 5
#define YELLOW 7
#define BOARD_COLOR ((BROWN << 4) | BLACK)

void init_video(void) {
  unsigned i;
  waitvsync();
  VERA.control = 0;
  VERA.irq_enable = 0;

  VERA.display.video = VGA_MODE;
  VERA.display.vscale = 64;
  VERA.display.hscale = 64;

  VERA.layer0.hscroll = 0;
  VERA.layer0.vscroll = 0;
  VERA.layer1.hscroll = 0;
  VERA.layer1.vscroll = 0;

  VERA.layer0.config =
      MAP_HEIGHT_32_TILES | MAP_WIDTH_64_TILES | MAP_COLOR_DEPTH_1BPP;
  VERA.layer1.config =
      MAP_HEIGHT_32_TILES | MAP_WIDTH_64_TILES | MAP_COLOR_DEPTH_1BPP;

  VERA.layer0.mapbase = LAYER0_MAP_ADDRESS >> 9;
  VERA.layer1.mapbase = LAYER1_MAP_ADDRESS >> 9;

  VERA.layer0.tilebase =
      ((TILE_BASE_ADDRESS >> 11) << 2) | TILE_WIDTH_8 | TILE_HEIGHT_8;
  VERA.layer1.tilebase =
      ((TILE_BASE_ADDRESS >> 11) << 2) | TILE_WIDTH_8 | TILE_HEIGHT_8;

  // Load charset
  VIA1.prb = DEFAULT_CHARS_BANK;

  VERA.address = TILE_BASE_ADDRESS;
  VERA.address_hi = VERA_INC_1;
  for (i = 0; i < 8 * 8 * 256; ++i) {
    VERA.data0 = ((char *)DEFAULT_CHARS_ADDR)[i];
  }
  VIA1.prb = 0;

  // Clear layers 0 and 1
  VERA.address = LAYER0_MAP_ADDRESS;
  VERA.address_hi = VERA_INC_1;
  for (i = 0; i < 32 * 64 * 2; i = i + 2) {
    VERA.data0 = ' ';
    VERA.data0 = (YELLOW << 4) | BLACK;
  }

  VERA.display.video = VGA_MODE | LAYER_0_ENABLE;
}

void write_string(const char *line, char x, char y, char layer1) {
  const char *cursor;
  VERA.address = LAYER0_MAP_ADDRESS + (64 * 2 * y) + x * 2;
  VERA.address_hi = VERA_INC_2;
  for (cursor = line; *cursor; ++cursor) {
    VERA.data0 = *cursor + 128;
  }
  VERA.address = 0;
}

void draw_horizontal_line(char value, char color, char x, char y, char width,
                          char layer1) {
  VERA.address = LAYER0_MAP_ADDRESS + (64 * 2 * y) + x * 2;
  VERA.address_hi = VERA_INC_1;
  while (width != 0) {
    VERA.data0 = value;
    VERA.data0 = color;
    --width;
  }
  VERA.address = 0;
}

void draw_vertical_line(char value, char color, char x, char y, char height,
                        char layer1) {
  char *address = LAYER0_MAP_ADDRESS + (64 * 2 * y) + x * 2;
  VERA.address = address;
  VERA.address_hi = VERA_INC_128;
  VERA.control = VERA_ADDR_2;
  VERA.address = address + 1;
  VERA.address_hi = VERA_INC_128;
  while (height != 0) {
    VERA.data0 = value;
    VERA.data1 = color;
    --height;
  }
  VERA.control = 0;
  VERA.address = 0;
}

void draw_tile(char value, char color, char x, char y, char layer1) {
  VERA.address = LAYER0_MAP_ADDRESS + (64 * 2 * y) + x * 2;
  VERA.address_hi = VERA_INC_1;
  VERA.data0 = value;
  VERA.data0 = color;
  VERA.address = 0;
}

void draw_board() {
  char x = 12;
  char y = 5;
  draw_horizontal_line(0x72, BOARD_COLOR, x, y, 17, 0);
  for (y = 6; y < (17 + 5 + 1); ++y) {
    draw_horizontal_line(0x5B, BOARD_COLOR, x, y, 17, 0);
  }
  draw_horizontal_line(0x71, BOARD_COLOR, x, y, 17, 0);

  draw_vertical_line(0x6B, BOARD_COLOR, 11, 6, 17, 0);
  draw_vertical_line(0x73, BOARD_COLOR, 29, 6, 17, 0);

  draw_tile(0x70, BOARD_COLOR, 11, 5, 0);
  draw_tile(0x6E, BOARD_COLOR, 29, 5, 0);
  draw_tile(0x6D, BOARD_COLOR, 11, 23, 0);
  draw_tile(0x7D, BOARD_COLOR, 29, 23, 0);

  write_string("weiqi16", 17, 1, 0);
  write_string("abcdefghijklmnopqrs", 11, 4, 0);
}

void main(void) {
  init_video();

  draw_board();

  while (1) {
    waitvsync();
  }
}
