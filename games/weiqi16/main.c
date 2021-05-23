#include <conio.h>
#include <cx16.h>

// -------------------- Constants -------------------------
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
#define DARK_GREY 11
#define LIGHT_GREY 15
#define GREY 12
#define ORANGE 8
#define BOARD_COLOR ((YELLOW << 4) | GREY)

#define DIRECTION_HORIZONTAL VERA_INC_2
#define DIRECTION_VERTICAL VERA_INC_128

#define BOARD_WIDTH 19
#define BOARD_HEIGHT 19

#define PIECE_WHITE_MASK 0b00000001
#define PIECE_BLACK_MASK 0b00000010
#define PIECE_LIVING_MASK 0b00000100

typedef unsigned char u8;
typedef unsigned int u16;

const char *board_label = "abcdefghijklmnopqrs";
const char *illegal_move = "illegal move";
const char *no_message = "            ";

#define STAR_POINT_CHAR 0x57

const u8 star_point_tile[8] = {
    0b00011000, 0b00011000, 0b00111100, 0b11111111,
    0b11111111, 0b00111100, 0b00011000, 0b00011000,
};

// -------------------- Global state -------------------------
char board[BOARD_HEIGHT][BOARD_WIDTH];
char move_input[3] = {' ', ' ', 0};
char move_input_cursor = 0;
char turn = PIECE_BLACK_MASK;
const char *message;
char ko_x = 0;
char ko_y = 0;

// -------------------- Prototypes -------------------------
char find_living_neighbors_of(char x, char y);

// -------------------- Video stuff -------------------------
void init_video(void) {
  unsigned i;
  char old_irq = VERA.irq_enable;
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

  // Add custom chars
  VERA.address = TILE_BASE_ADDRESS + (STAR_POINT_CHAR * 8);
  VERA.address_hi = VERA_INC_1;
  for (i = 0; i < 8; ++i) {
    VERA.data0 = star_point_tile[i];
  }

  // Clear layers 0 and 1
  VERA.address = LAYER0_MAP_ADDRESS;
  VERA.address_hi = VERA_INC_1;
  for (i = 0; i < 32 * 64 * 2; i = i + 2) {
    VERA.data0 = ' ';
    VERA.data0 = (ORANGE << 4) | BLACK;
  }
  for (i = 32 * 64 * 2; i < 32 * 64 * 2 * 2; i = i + 2) {
    VERA.data0 = ' ';
    VERA.data0 = 0;
  }

  VERA.display.video = VGA_MODE | LAYER_0_ENABLE | LAYER_1_ENABLE;
  VERA.irq_enable = old_irq;
}

void write_string(const char *line, char x, char y, char direction,
                  char layer1) {
  const char *cursor;
  VERA.address = LAYER0_MAP_ADDRESS + (64 * 2 * y) + x * 2;
  VERA.address_hi = direction;
  for (cursor = line; *cursor; ++cursor) {
    VERA.data0 = *cursor + 128;
  }
  VERA.address = 0;
}

void draw_line(char value, char color, char x, char y, char length,
               char direction, char layer1) {
  char *address = (char *)(LAYER0_MAP_ADDRESS + (64 * 2 * y) + x * 2);
  VERA.address = (u16)address;
  VERA.address_hi = direction;
  VERA.control = VERA_ADDR_2;
  VERA.address = (u16)address + 1;
  VERA.address_hi = direction;
  while (length != 0) {
    VERA.data0 = value;
    VERA.data1 = color;
    --length;
  }
  VERA.control = 0;
  VERA.address = 0;
}

void draw_tile(char value, char color, char x, char y, char layer1) {
  if (layer1 == 0) {
    VERA.address = LAYER0_MAP_ADDRESS + (64 * y + x) * 2;
  } else {
    VERA.address = LAYER1_MAP_ADDRESS + (64 * y + x) * 2;
  }
  VERA.address_hi = VERA_INC_1;
  VERA.data0 = value;
  VERA.data0 = color;
  VERA.address = 0;
}

void draw_board() {
  char x = 12;
  char y = 5;
  draw_line(0x72, BOARD_COLOR, x, y, 17, DIRECTION_HORIZONTAL, 0);
  for (y = 6; y < (17 + 5 + 1); ++y) {
    draw_line(0x5B, BOARD_COLOR, x, y, 17, DIRECTION_HORIZONTAL, 0);
  }
  draw_line(0x71, BOARD_COLOR, x, y, 17, DIRECTION_HORIZONTAL, 0);

  for (y = 3; y < 19; y += 6) {
    for (x = 3; x < 19; x += 6) {
      draw_tile(STAR_POINT_CHAR, BOARD_COLOR, x + 11, y + 5, 0);
    }
  }

  draw_line(0x6B, BOARD_COLOR, 11, 6, 17, DIRECTION_VERTICAL, 0);
  draw_line(0x73, BOARD_COLOR, 29, 6, 17, DIRECTION_VERTICAL, 0);

  draw_tile(0x70, BOARD_COLOR, 11, 5, 0);
  draw_tile(0x6E, BOARD_COLOR, 29, 5, 0);
  draw_tile(0x6D, BOARD_COLOR, 11, 23, 0);
  draw_tile(0x7D, BOARD_COLOR, 29, 23, 0);

  write_string("weiqi16", 17, 1, DIRECTION_HORIZONTAL, 0);
  write_string(board_label, 11, 4, DIRECTION_HORIZONTAL, 0);
  write_string(board_label, 10, 5, DIRECTION_VERTICAL, 0);
}

void draw_piece(char color, char x, char y) {
  draw_tile(0x51, (BLACK << 4) | color, x, y, 1);
}

void draw_pieces() {
  char x, y, piece;
  for (y = 0; y < BOARD_HEIGHT; ++y) {
    for (x = 0; x < BOARD_WIDTH; ++x) {
      piece = board[y][x];

      if (piece & PIECE_BLACK_MASK) {
        draw_piece(DARK_GREY, x + 11, y + 5);
      } else if (piece & PIECE_WHITE_MASK) {
        draw_piece(WHITE, x + 11, y + 5);
      } else {
        draw_tile(' ', 0, x + 11, y + 5, 1);
      }
    }
  }
}

// -------------------- Gameplay stuff -------------------------

unsigned char mark_dead_stones_for(char player) {
  unsigned char dead_stones_found = 0;
  char found_new_living;
  char x, y, board_value;

  // Reset living flag on all spaces
  for (y = 0; y < BOARD_HEIGHT; ++y) {
    for (x = 0; x < BOARD_WIDTH; ++x) {
      board_value = board[y][x];
      if ((board_value & (PIECE_WHITE_MASK | PIECE_BLACK_MASK)) != 0) {
        board[y][x] = board_value & ~(PIECE_LIVING_MASK);
      } else {
        board[y][x] = PIECE_LIVING_MASK;
      }
    }
  }

  // Keep marking living stones until no more work is done
  do {
    found_new_living = 0;
    for (y = 0; y < BOARD_HEIGHT; ++y) {
      for (x = 0; x < BOARD_WIDTH; ++x) {
        board_value = board[y][x];
        if (board_value & player) {
          if ((board_value & PIECE_LIVING_MASK) == 0) {
            if (find_living_neighbors_of(x, y) != 0) {
              board[y][x] = board_value | PIECE_LIVING_MASK;
              found_new_living = 1;
            }
          }
        }
      }
    }
  } while (found_new_living);

  // Count dead stones
  for (y = 0; y < BOARD_HEIGHT; ++y) {
    for (x = 0; x < BOARD_WIDTH; ++x) {
      if ((board[y][x] & (player | PIECE_LIVING_MASK)) == player) {
        ++dead_stones_found;
      }
    }
  }
  return dead_stones_found;
}

char find_living_neighbors_of(char x, char y) {
  if (y < BOARD_HEIGHT - 1) {
    if ((board[y + 1][x] & PIECE_LIVING_MASK) == PIECE_LIVING_MASK) {
      return 1;
    }
  }
  if (x < BOARD_WIDTH - 1) {
    if (board[y][x + 1] & PIECE_LIVING_MASK) {
      return 1;
    }
  }
  if (y > 0) {
    if (board[y - 1][x] & PIECE_LIVING_MASK) {
      return 1;
    }
  }
  if (x > 0) {
    if (board[y][x - 1] & PIECE_LIVING_MASK) {
      return 1;
    }
  }
  return 0;
}

void sweep_dead_stones_for(char player, u8 dead_stone_count) {
  char x, y;
  for (y = 0; y < BOARD_HEIGHT; ++y) {
    for (x = 0; x < BOARD_WIDTH; ++x) {
      if ((board[y][x] & (player | PIECE_LIVING_MASK)) == player) {
        board[y][x] = 0;
        if (dead_stone_count == 1) {
          ko_x = x;
          ko_y = y;
        }
      }
    }
  }
}

void try_move(char x, char y) {
  char opposing_player;
  unsigned char dead_stones;
  char current_value = board[y][x];

  if (turn == PIECE_BLACK_MASK) {
    opposing_player = PIECE_WHITE_MASK;
  } else {
    opposing_player = PIECE_BLACK_MASK;
  }

  if (current_value & (PIECE_BLACK_MASK | PIECE_WHITE_MASK)) {
    message = illegal_move;
    return;
  }

  if (x == ko_x && y == ko_y) {
    message = illegal_move;
    return;
  }

  board[y][x] = turn;

  dead_stones = mark_dead_stones_for(opposing_player);
  if (dead_stones == 0) {
    // If we didn't kill anything, we need to check for suicide move
    dead_stones = mark_dead_stones_for(turn);
    if (dead_stones > 0) {
      message = illegal_move;
      board[y][x] = 0;
      return;
    }
  }

  ko_x = BOARD_WIDTH + 1;
  ko_y = BOARD_HEIGHT + 1;
  if (dead_stones > 0) {
    sweep_dead_stones_for(opposing_player, dead_stones);
  }

  message = no_message;

  // Move was OK, commit it
  draw_pieces();
  if (turn == PIECE_BLACK_MASK) {
    draw_piece(WHITE, 1, 26);
  } else {
    draw_piece(DARK_GREY, 1, 26);
  }
  turn = opposing_player;
}

void play_game() {
  draw_board();
  draw_pieces();

  message = no_message;

  write_string("move:", 2, 26, DIRECTION_HORIZONTAL, 0);
  draw_piece(DARK_GREY, 1, 26);

  try_move(7, 6);
  try_move(6, 6);
  try_move(7, 8);
  try_move(6, 8);
  try_move(8, 7);
  try_move(7, 7);
  try_move(1, 1);
  try_move(5, 7);

  while (1) {
    if (kbhit()) {
      char c = cgetc();
      draw_tile(c, BOARD_COLOR, 0, 0, 0);

      if (c >= 0x41 && c < 0x54) {
        if (move_input_cursor < 2) {
          move_input[move_input_cursor] = c;
          ++move_input_cursor;
        }
      } else {
        switch (c) {
        case 0x14:
          if (move_input_cursor > 0) {
            move_input[--move_input_cursor] = ' ';
          }
          break;
        case 0x0D:
          if (move_input_cursor == 2) {
            try_move(move_input[0] - 0x41, move_input[1] - 0x41);
            move_input_cursor = 0;
            move_input[0] = ' ';
            move_input[1] = ' ';
          }
          break;
        }
      }
      if (move_input_cursor == 2) {
        draw_piece(LIGHT_GREY, move_input[0] - 0x41 + 11,
                   move_input[1] - 0x41 + 5);
      } else {
        draw_pieces();
      }
      write_string(move_input, 7, 26, DIRECTION_HORIZONTAL, 0);
      write_string(message, 7, 27, DIRECTION_HORIZONTAL, 0);
    }

    waitvsync();
  }
}

void main(void) {
  init_video();
  draw_board();
  draw_pieces();

  while (1) {
    play_game();
  }
}
