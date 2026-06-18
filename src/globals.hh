#include <stdio.h>

#define PORT 1337
#define MOVE_BUF_LEN 10
#define TCP_BUF_LEN 1000

#define no_msg() (errno == EAGAIN || errno == EWOULDBLOCK)

namespace chess {

typedef enum {
	PIECE_EMPTY,
	PIECE_PAWN,
	PIECE_BISHOP,
	PIECE_KNIGHT,
	PIECE_ROOK,
	PIECE_QUEEN,
	PIECE_KING
} PIECE_TYPE;

typedef enum {
	NONE,
	WHITE,
	BLACK
} COLOR;

class Pos {
  public:
	int x;
	int y;

	Pos() {}
	Pos(int x, int y) {
		this->x = x;
		this->y = y;
	}

	// if the move is in the board
	bool in_board() {
		if (x >= 0 && x < 8 && y >= 0 && y < 8) return true;
		return false;
	}
};

void repeat_char(char c, int count) {
	for (int i = 0; i < count; i++) {
		printf("%c", c);
	}
}

class Piece {
  public:
	PIECE_TYPE type;
	COLOR color;

	Piece() {
		type = PIECE_EMPTY;
		color = NONE;
	}

	Piece(PIECE_TYPE type, COLOR is_white) {
		this->type = type;
		color = is_white;
	}
};

char piece_codes[] = {' ', 'p', 'b', 'n', 'r', 'q', 'k'};
const char *color_codes[] = {"empty", "white", "black"};
int pawn_dir[] = {0, 1, -1};

void board_to_binary(Piece board[8][8], int *board_binary) {
	int col_bytes = 2;
	int row_bytes = 8 * col_bytes;

	int piece = 0;
	int color = 1;

	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			board_binary[row_bytes * y + col_bytes * x + piece] = board[y][x].type;
			board_binary[row_bytes * y + col_bytes * x + color] = board[y][x].color;
		}
	}
}

void binary_to_board(Piece board[8][8], int *board_binary) {
	int col_bytes = 2;
	int row_bytes = 8 * col_bytes;

	int piece = 0;
	int color = 1;

	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			board[y][x].type = (PIECE_TYPE)board_binary[row_bytes * y + col_bytes * x + piece];
			board[y][x].color = (COLOR)board_binary[row_bytes * y + col_bytes * x + color];
		}
	}
}
} // namespace chess