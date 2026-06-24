#include <stdarg.h>
#include <stdio.h>
#include <vector>

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

int piece_values[] = {0, 1, 3, 3, 5, 9, 100000};

typedef enum {
	NONE,
	WHITE,
	BLACK
} COLOR;

typedef struct {
	COLOR turn; // the player's turn
	COLOR you;	// the player being sent to
	int board[8][8][2];
	int last_move[2][2];
	PIECE_TYPE captured;
} packet;

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
		return x >= 0 && x < 8 && y >= 0 && y < 8;
	}
};

class Move {
  public:
	Pos start;
	Pos end;

	Move() {
	}

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

	Piece(PIECE_TYPE type, COLOR color) {
		this->type = type;
		this->color = color;
	}

	static COLOR opp(COLOR color) {
		if (color == WHITE) return BLACK;
		else if (color == BLACK) return WHITE;
		return NONE;
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
	COLOR me;
	Logger *logger;
	bool stage = true;
	Move last_move;
	PIECE_TYPE captured;

	bool allow_castle_left_white = true;
	bool allow_castle_left_black = true;
	bool allow_castle_right_white = true;
	bool allow_castle_right_black = true;

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

	int get_score() {
		int score = 0;
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				score += piece_values[rows[y][x].type] * pawn_dir[rows[y][x].color];
			}
		}

		return score;
	}

	void check_castle_rights(Pos pos) {
		// a rook position
		if (pos.x == 0) {
			if (pos.y == 0) allow_castle_left_white = false;
			else if (pos.y == 7) allow_castle_left_black = false;
		} else if (pos.x == 7) {
			if (pos.y == 0) allow_castle_right_white = false;
			else if (pos.y == 7) allow_castle_right_black = false;
		} else if (pos.x == 4) {
			// king position
			if (pos.y == 0) {
				allow_castle_left_white = false;
				allow_castle_right_white = false;
			} else if (pos.y == 7) {
				allow_castle_left_black = false;
				allow_castle_right_black = false;
			}
		}
	}

	// commits move and returns change in score
	int commit(Move move) {
		Pos start = move.start;
		Pos end = move.end;

		captured = rows[end.y][end.x].type;

		if (rows[start.y][start.x].type == PIECE_KING) {
			if (end.x - start.x == 2) {
				// king side castle
				rows[start.y][start.x + 1] = rows[start.y][7];
				rows[start.y][7] = Piece();
			} else if (end.x - start.x == -2) {
				// queen side castle
				rows[start.y][start.x - 1] = rows[start.y][0];
				rows[start.y][0] = Piece();
			}
		}

		check_castle_rights(start);
		check_castle_rights(end);

		int change = pawn_dir[player_turn] * piece_values[captured];

		rows[end.y][end.x] = rows[start.y][start.x];
		rows[start.y][start.x] = Piece();
		if (rows[end.y][end.x].type == PIECE_PAWN && (end.y == 0 || end.y == 7)) {
			rows[end.y][end.x].type = PIECE_QUEEN;

			change += piece_values[rows[end.y][end.x].type] - 1; // remove a pawn and add the promoted piece
		}

		if (stage) {
			char str[12] = "";
			move.to_string(str);
			logger->note("made move \"%s\", score = %i", str, get_score());
		}

		last_move = move;

		return change;
	}

	bool validate(std::vector<Move> &moves, Move move) {
		for (size_t i = 0; i < moves.size(); i++) {
			if (moves[i] == move) return true;
		}

		return false;
	}

	bool is_turn(Pos pos) {
		return pos.in_board() && rows[pos.y][pos.x].color == player_turn;
	}

	bool is_opp(Pos pos) {
		return pos.in_board() && rows[pos.y][pos.x].color != player_turn && !is_empty(pos);
	}

	bool is_me(Pos pos) {
		return pos.in_board() && rows[pos.y][pos.x].color == player_turn;
	}

	bool is_empty(Pos pos) {
		return pos.in_board() && rows[pos.y][pos.x].color == NONE || rows[pos.y][pos.x].type == PIECE_EMPTY;
	}

	bool load_board(const char *filename) {
		FILE *file = fopen(filename, "r");

		if (file == NULL) {
			logger->err("could not open board");
			return false;
		}

		size_t last_size = 0;
		char buf[128];

		fgets(buf, sizeof(buf), file);
		sscanf(buf, "%i", &player_turn);

		allow_castle_left_white = fgetc(file) == '1';
		fgetc(file);
		allow_castle_right_white = fgetc(file) == '1';
		fgetc(file);
		allow_castle_left_black = fgetc(file) == '1';
		fgetc(file);
		allow_castle_right_black = fgetc(file) == '1';
		fgets(buf, sizeof(buf), file);

		for (int y = 7; y >= 0; y--) {
			for (int x = 0; x < 8; x++) {
				char piece = fgetc(file) - '0';
				char color = fgetc(file) - '0';
				if (x < 7) fgetc(file); // get the space
				rows[y][x] = Piece((PIECE_TYPE)piece, (COLOR)color);
			}

			fgets(buf, sizeof(buf), file);
		}

		fclose(file);

		return true;
	}

	bool init() {
		last_move.start = Pos(-1, -1);
		last_move.end = Pos(-1, -1);

		clear();
		if (!load_board("boards/start.txt")) return false;

		return true;
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

	int iterate_move_protected(Pos start, int dx, int dy) {
		Pos pos = start;
		pos.x += dx;
		pos.y += dy;

		while (pos.x < 8 && pos.x >= 0 && pos.y < 8 && pos.y >= 0) {
			if (!is_empty(pos)) {
				// the line is interrupted
				if (is_me(pos)) return 1;
				return 0;
			}

			pos.x += dx;
			pos.y += dy;
		}

		if (is_me(pos)) return 1;
		return 0;
	}

	int protected_pieces() {
		int count = 0;
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				if (rows[y][x].color == player_turn) {
					Pos start = Pos(x, y);

					switch (rows[start.y][start.x].type) {
					case PIECE_PAWN:

						if (is_me(Pos(x + 1, y + UP()))) count++;
						if (is_me(Pos(x - 1, y + UP()))) count++;

						break;

					case PIECE_BISHOP:
						count += iterate_move_protected(start, 1, 1);
						count += iterate_move_protected(start, 1, -1);
						count += iterate_move_protected(start, -1, 1);
						count += iterate_move_protected(start, -1, -1);

						break;

					case PIECE_ROOK:
						count += iterate_move_protected(start, 0, 1);
						count += iterate_move_protected(start, 0, -1);
						count += iterate_move_protected(start, 1, 0);
						count += iterate_move_protected(start, -1, 0);

						break;

					case PIECE_KNIGHT:
						if (is_me(Pos(x + 1, y + 2))) count++;
						if (is_me(Pos(x + 1, y - 2))) count++;
						if (is_me(Pos(x - 1, y + 2))) count++;
						if (is_me(Pos(x - 1, y - 2))) count++;

						if (is_me(Pos(x + 2, y + 1))) count++;
						if (is_me(Pos(x + 2, y - 1))) count++;
						if (is_me(Pos(x - 2, y + 1))) count++;
						if (is_me(Pos(x - 2, y - 1))) count++;

						break;

					case PIECE_QUEEN:
						count += iterate_move_protected(start, 1, 1);
						count += iterate_move_protected(start, 1, -1);
						count += iterate_move_protected(start, -1, 1);
						count += iterate_move_protected(start, -1, -1);

						count += iterate_move_protected(start, 0, 1);
						count += iterate_move_protected(start, 0, -1);
						count += iterate_move_protected(start, 1, 0);
						count += iterate_move_protected(start, -1, 0);

						break;

					case PIECE_KING:
						if (is_me(Pos(x, y + 1))) count++;
						if (is_me(Pos(x, y - 1))) count++;
						if (is_me(Pos(x + 1, y))) count++;
						if (is_me(Pos(x - 1, y))) count++;

						if (is_me(Pos(x + 1, y + 1))) count++;
						if (is_me(Pos(x + 1, y - 1))) count++;
						if (is_me(Pos(x - 1, y + 1))) count++;
						if (is_me(Pos(x - 1, y - 1))) count++;

						break;

					default:
						break;
					}
				}
			}
		}

		return count;
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
						if ((y == 1 || y == 6) && is_empty(Pos(x, y + UP())) && is_empty(Pos(x, y + UPP()))) add_move(moves, Move(start, Pos(x, y + UPP())));

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

						// TODO: check that the move does not cross check
						if ((player_turn == WHITE && allow_castle_right_white) || (player_turn == BLACK && allow_castle_right_black)) {
							if (is_empty(Pos(5, y)) && is_empty(Pos(6, y))) add_move(moves, Move(start, Pos(6, y)));
						}
						if ((player_turn == WHITE && allow_castle_left_white) || (player_turn == BLACK && allow_castle_left_black)) {
							if (is_empty(Pos(3, y)) && is_empty(Pos(2, y)) && is_empty(Pos(1, y))) add_move(moves, Move(start, Pos(2, y)));
						}

						break;

					default:
						break;
					}
				}
			}
		}

		timespec_get(&end, TIME_UTC);
		if (stage) {
			// char str[12] = "";
			// for (size_t i = 0; i < moves.size(); i++) {
			// 	moves[i].to_string(str);
			// 	printf("%s\n", str);
			// }

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

void board_to_packet(Board &board, packet &board_binary) {
	board_binary.turn = board.player_turn;
	board_binary.last_move[0][0] = board.last_move.start.x;
	board_binary.last_move[0][1] = board.last_move.start.y;
	board_binary.last_move[1][0] = board.last_move.end.x;
	board_binary.last_move[1][1] = board.last_move.end.y;
	board_binary.captured = board.captured;

	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			board_binary.board[y][x][0] = board[y][x].type;
			board_binary.board[y][x][1] = board[y][x].color;
		}
	}
}

void packet_to_board(Board &board, packet &board_binary) {
	board.me = board_binary.you;
	board.player_turn = board_binary.turn;
	board.last_move.start.x = board_binary.last_move[0][0];
	board.last_move.start.y = board_binary.last_move[0][1];
	board.last_move.end.x = board_binary.last_move[1][0];
	board.last_move.end.y = board_binary.last_move[1][1];
	board.captured = board_binary.captured;

	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			board[y][x].type = (PIECE_TYPE)board_binary.board[y][x][0];
			board[y][x].color = (COLOR)board_binary.board[y][x][1];
		}
	}
}
} // namespace chess