#include <stdio.h>
#include <vector>

#include "logger.hh"

#define PORT 1338
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

	bool operator==(Pos pos) {
		return (x == pos.x && y == pos.y);
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

	bool operator==(Move move) {
		return (start == move.start && end == move.end);
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
	bool stage = true;

	BoardRow &operator[](int index) {
		return rows[index];
	}

	Piece &operator[](Pos pos) {
		return rows[pos.y][pos.x];
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

	bool validate(std::vector<Move> &moves, Move move) {
		for (size_t i = 0; i < moves.size(); i++) {
			if (moves[i] == move) return true;
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

	// this evaluates what moves are allowed if the given move was made
	bool stage_move(Move move) {
		Board staging_board = *this;
		staging_board.stage = false; // the staging board should not recursively stage more boards
		staging_board.commit(move);
		staging_board.next_turn();

		std::vector<Move> moves = staging_board.get_valid_moves();

		// check if the move allows the opponent to take the king
		for (size_t i = 0; i < moves.size(); i++) {
			if (staging_board[moves[i].end].type == PIECE_KING) return false;
		}

		return true;
	}

	void add_move(std::vector<Move> &moves, Move move) {
		if (move.end.in_board() && (is_empty(move.end) || is_opp(move.end))) {
			if (!stage || stage_move(move)) moves.push_back(move);
		}
	}

	void iterate_move(std::vector<Move> &moves, Pos start, int dx, int dy) {
		Pos pos = start;
		pos.x += dx;
		pos.y += dy;

		while (pos.x < 8 && pos.x >= 0 && pos.y < 8 && pos.y >= 0) {
			add_move(moves, Move(start, pos));
			if (!is_empty(pos)) {
				// the line is interrupted
				return;
			}

			pos.x += dx;
			pos.y += dy;
		}

		// add the final position
		add_move(moves, Move(start, pos));
	}

	std::vector<Move> get_valid_moves() {
		struct timespec start;
		struct timespec end;
		timespec_get(&start, TIME_UTC);

		std::vector<Move> moves;
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				if (rows[y][x].color == player_turn) {
					Pos start = Pos(x, y);

					switch (rows[start.y][start.x].type) {
					case PIECE_PAWN:

						if (is_empty(Pos(x, y + UP()))) add_move(moves, Move(start, Pos(x, y + UP())));
						if (is_opp(Pos(x + 1, y + UP()))) add_move(moves, Move(start, Pos(x + 1, y + UP())));
						if (is_opp(Pos(x - 1, y + UP()))) add_move(moves, Move(start, Pos(x - 1, y + UP())));
						if (is_empty(Pos(x, y + UP())) && is_empty(Pos(x, y + UPP()))) add_move(moves, Move(start, Pos(x, y + UPP())));

						break;

					case PIECE_BISHOP:
						iterate_move(moves, start, 1, 1);
						iterate_move(moves, start, 1, -1);
						iterate_move(moves, start, -1, 1);
						iterate_move(moves, start, -1, -1);

						break;

					case PIECE_ROOK:
						iterate_move(moves, start, 0, 1);
						iterate_move(moves, start, 0, -1);
						iterate_move(moves, start, 1, 0);
						iterate_move(moves, start, -1, 0);

						break;

					case PIECE_KNIGHT:
						add_move(moves, Move(start, Pos(x + 1, y + 2)));
						add_move(moves, Move(start, Pos(x + 1, y - 2)));
						add_move(moves, Move(start, Pos(x - 1, y + 2)));
						add_move(moves, Move(start, Pos(x - 1, y - 2)));

						add_move(moves, Move(start, Pos(x + 2, y + 1)));
						add_move(moves, Move(start, Pos(x + 2, y - 1)));
						add_move(moves, Move(start, Pos(x - 2, y + 1)));
						add_move(moves, Move(start, Pos(x - 2, y - 1)));

						break;

					case PIECE_QUEEN:
						iterate_move(moves, start, 1, 1);
						iterate_move(moves, start, 1, -1);
						iterate_move(moves, start, -1, 1);
						iterate_move(moves, start, -1, -1);

						iterate_move(moves, start, 0, 1);
						iterate_move(moves, start, 0, -1);
						iterate_move(moves, start, 1, 0);
						iterate_move(moves, start, -1, 0);

						break;

					case PIECE_KING:
						add_move(moves, Move(start, Pos(x, y + 1)));
						add_move(moves, Move(start, Pos(x, y - 1)));
						add_move(moves, Move(start, Pos(x + 1, y)));
						add_move(moves, Move(start, Pos(x - 1, y)));

						add_move(moves, Move(start, Pos(x + 1, y + 1)));
						add_move(moves, Move(start, Pos(x + 1, y - 1)));
						add_move(moves, Move(start, Pos(x - 1, y + 1)));
						add_move(moves, Move(start, Pos(x - 1, y - 1)));

						break;

					default:
						break;
					}
				}
			}
		}

		timespec_get(&end, TIME_UTC);
		if (stage) {
			char str[12] = "";
			for (size_t i = 0; i < moves.size(); i++) {
				moves[i].to_string(str);
				printf("%s\n", str);
			}

			logger->note("found %li valid moves (%lins)", moves.size(), 1000000000 * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec));
		}

		if (moves.size() == 0) {
			// TODO: handle checkmate
		}

		return moves;
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