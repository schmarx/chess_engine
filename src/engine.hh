#include <stdio.h>
#include <stdlib.h>

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

char codes[] = {' ', 'p', 'b', 'n', 'r', 'q', 'k'};

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

class engine {
  public:
	bool white_turn;

	Piece board[8][8];
	engine() {
		init_board();
	}

	void init_board() {
		clear_board();

		for (int x = 0; x < 8; x++) {
			board[1][x] = Piece(PIECE_PAWN, WHITE);
			board[6][x] = Piece(PIECE_PAWN, BLACK);
		}

		board[0][0] = Piece(PIECE_ROOK, WHITE);
		board[0][7] = Piece(PIECE_ROOK, WHITE);
		board[0][1] = Piece(PIECE_BISHOP, WHITE);
		board[0][6] = Piece(PIECE_BISHOP, WHITE);
		board[0][2] = Piece(PIECE_KNIGHT, WHITE);
		board[0][5] = Piece(PIECE_KNIGHT, WHITE);
		board[0][3] = Piece(PIECE_QUEEN, WHITE);
		board[0][4] = Piece(PIECE_KING, WHITE);

		board[7][0] = Piece(PIECE_ROOK, BLACK);
		board[7][7] = Piece(PIECE_ROOK, BLACK);
		board[7][1] = Piece(PIECE_BISHOP, BLACK);
		board[7][6] = Piece(PIECE_BISHOP, BLACK);
		board[7][2] = Piece(PIECE_KNIGHT, BLACK);
		board[7][5] = Piece(PIECE_KNIGHT, BLACK);
		board[7][4] = Piece(PIECE_QUEEN, BLACK);
		board[7][3] = Piece(PIECE_KING, BLACK);
		white_turn = true;
	}
	void clear_board() {
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				board[y][x] = Piece(PIECE_EMPTY, NONE);
			}
		}
	}

	void print_board() {
		for (int y = 7; y >= 0; y--) {
			for (int x = 0; x < 8; x++) {
				printf(" %c ", codes[board[y][x].type]);
			}
			printf("\n\n");
		}
	}

	void run() {
		print_board();
	}
};
