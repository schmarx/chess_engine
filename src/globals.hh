#include <stdio.h>

#include "logger.hh"

#define PORT 1337
#define MOVE_BUF_LEN 10
#define TCP_BUF_LEN 1000

#define no_msg() (errno == EAGAIN || errno == EWOULDBLOCK)
#define UP() pawn_dir[player_turn]
#define UPP() (2 * pawn_dir[player_turn])

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

char piece_codes[] = {' ', 'p', 'b', 'n', 'r', 'q', 'k'};
const char *color_codes[] = {"empty", "white", "black"};
int pawn_dir[] = {0, 1, -1};

class Pos {
  public:
	int x;
	int y;

	Pos() {}
	Pos(int x, int y) {
		this->x = x;
		this->y = y;
	}

	Pos(char x, int y) {
		this->x = x - 'a';
		this->y = y - 1;
	}

	// if the move is in the board
	bool in_board() {
		if (x >= 0 && x < 8 && y >= 0 && y < 8) return true;
		return false;
	}
};

class Move {
  public:
	Pos start;
	Pos end;
	Move(char *input) {
		char x1;
		char x2;
		int y1;
		int y2;

		sscanf(input, "%c%i%c%i", &x1, &y1, &x2, &y2);
		Pos pos1 = Pos(x1, y1);
		Pos pos2 = Pos(x2, y2);

		start = pos1;
		end = pos2;
	}

	Move(Pos start, Pos end) {
		this->start = start;
		this->end = end;
	}

	void to_string(char *str) {
		str[0] = start.x + 'a';
		str[1] = start.y + '1';
		str[2] = end.x + 'a';
		str[3] = end.y + '1';
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

class BoardRow {
  private:
	Piece pieces[8];

  public:
	Piece &operator[](int index) {
		return pieces[index];
	}
};

class Board {
  private:
	BoardRow rows[8];

  public:
	COLOR player_turn;
	Logger *logger;

	BoardRow &operator[](int index) {
		return rows[index];
	}

	void next_turn() {
		if (player_turn == WHITE) player_turn = BLACK;
		else player_turn = WHITE;
	}

	void commit(Move move) {
		Pos start = move.start;
		Pos end = move.end;

		rows[end.y][end.x] = rows[start.y][start.x];
		rows[start.y][start.x] = Piece();
	}

	bool validate_pawn(Pos start, Pos end) {
		// TODO: check en passant and promotion
		bool same_col = (start.x == end.x);
		bool one_up = (end.y - start.y == UP());
		bool two_up = (end.y - start.y == UPP());

		bool one_side = abs(start.x - end.x) == 1;

		if (same_col && (one_up || two_up)) {
			Pos above = Pos(start.x, start.y + UP());
			if (!is_empty(end) || (two_up && !is_empty(above))) {
				logger->err("piece in the way");
				return false;
			}

		} else if (one_side && one_up) {
			if (!is_opp(end)) {
				logger->err("no piece to capture");
				return false;
			}
		} else {
			logger->err("invalid destination");
			return false;
		}

		return true;
	}

	bool check_line(Pos start, Pos end) {
		int dx = end.x - start.x;
		int dy = end.y - start.y;

		int step_x = dx / abs(dx);
		int step_y = dy / abs(dy);

		if (dx == 0) step_x = 0;
		if (dy == 0) step_y = 0;

		Pos pos = Pos(start.x + step_x, start.y + step_y);
		while (pos.x != end.x || pos.y != end.y) {
			if (!is_empty(pos)) {
				logger->err("piece in the way");
				return false;
			}

			pos.x += step_x;
			pos.y += step_y;
		}

		if (!is_empty(end) && !is_opp(end)) {
			logger->err("cannot capture own piece");
			return false;
		}

		return true;
	}

	bool validate_bishop(Pos start, Pos end) {
		int dx = end.x - start.x;
		int dy = end.y - start.y;

		if (abs(dx) != abs(dy)) {
			logger->err("move must be diagonal");
			return false;
		}
		return check_line(start, end);
	}

	bool validate_rook(Pos start, Pos end) {
		int dx = end.x - start.x;
		int dy = end.y - start.y;

		if (dx != 0 && dy != 0) {
			logger->err("move must be along an axis");
			return false;
		}
		return check_line(start, end);
	}

	bool validate_knight(Pos start, Pos end) {
		int dx = end.x - start.x;
		int dy = end.y - start.y;

		if ((abs(dx) == 1 && abs(dy) == 2) || (abs(dx) == 2 && abs(dy) == 1)) {
			if (!is_empty(end) && !is_opp(end)) {
				logger->err("cannot capture own piece");
				return false;
			}

			return true;
		}

		logger->err("invalid move");
		return false;
	}

	bool validate_queen(Pos start, Pos end) {
		if (validate_bishop(start, end) || validate_rook(start, end)) {
			return true;
		}

		return false;
	}

	bool validate_king(Pos start, Pos end) {
		int dx = end.x - start.x;
		int dy = end.y - start.y;
		if (abs(dx) > 1 || abs(dy) > 1) {
			// TODO: check for castle
			logger->err("must move to adjacent square");
			return false;
		}

		return true;
	}

	bool validate(Move move) {
		Pos start = move.start;
		Pos end = move.end;
		switch (rows[start.y][start.x].type) {
		case PIECE_PAWN:
			return validate_pawn(start, end);
			break;

		case PIECE_BISHOP:
			return validate_bishop(start, end);
			break;

		case PIECE_ROOK:
			return validate_rook(start, end);
			break;

		case PIECE_KNIGHT:
			return validate_knight(start, end);
			break;

		case PIECE_QUEEN:
			return validate_queen(start, end);
			break;

		case PIECE_KING:
			return validate_king(start, end);
			break;

		default:
			break;
		}

		return false;
	}

	bool is_turn(Pos pos) {
		return rows[pos.y][pos.x].color == player_turn;
	}

	bool is_opp(Pos pos) {
		return rows[pos.y][pos.x].color != player_turn && !is_empty(pos);
	}

	bool is_empty(Pos pos) {
		return rows[pos.y][pos.x].color == NONE || rows[pos.y][pos.x].type == PIECE_EMPTY;
	}

	void init() {
		clear();

		for (int x = 0; x < 8; x++) {
			rows[1][x] = Piece(PIECE_PAWN, WHITE);
			rows[6][x] = Piece(PIECE_PAWN, BLACK);
		}

		rows[0][0] = Piece(PIECE_ROOK, WHITE);
		rows[0][7] = Piece(PIECE_ROOK, WHITE);
		rows[0][2] = Piece(PIECE_BISHOP, WHITE);
		rows[0][5] = Piece(PIECE_BISHOP, WHITE);
		rows[0][1] = Piece(PIECE_KNIGHT, WHITE);
		rows[0][6] = Piece(PIECE_KNIGHT, WHITE);
		rows[0][3] = Piece(PIECE_QUEEN, WHITE);
		rows[0][4] = Piece(PIECE_KING, WHITE);

		rows[7][0] = Piece(PIECE_ROOK, BLACK);
		rows[7][7] = Piece(PIECE_ROOK, BLACK);
		rows[7][2] = Piece(PIECE_BISHOP, BLACK);
		rows[7][5] = Piece(PIECE_BISHOP, BLACK);
		rows[7][1] = Piece(PIECE_KNIGHT, BLACK);
		rows[7][6] = Piece(PIECE_KNIGHT, BLACK);
		rows[7][3] = Piece(PIECE_QUEEN, BLACK);
		rows[7][4] = Piece(PIECE_KING, BLACK);

		player_turn = WHITE;
	}

	void clear() {
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				rows[y][x] = Piece(PIECE_EMPTY, NONE);
			}
		}
	}

	void draw() {
		printf("  ");
		repeat_char('_', 24 + 4);
		printf("\n");

		for (int y = 7; y >= 0; y--) {
			printf("%i | ", y + 1);
			for (int x = 0; x < 8; x++) {
				printf(" %c ", piece_codes[rows[y][x].type]);
			}
			printf(" |\n");

			printf("  | ");
			repeat_char(' ', 24);
			printf(" |\n");
		}

		printf("  ");
		repeat_char('_', 24 + 4);
		printf("\n");
		printf("     a  b  c  d  e  f  g  h\n");
	}
};

void board_to_binary(Board board, int *board_binary) {
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

void binary_to_board(Board &board, int *board_binary) {
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