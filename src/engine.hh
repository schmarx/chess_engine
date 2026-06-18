#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <vector>

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "globals.hh"

namespace chess {

class engine {
  public:
	COLOR player_turn;
	bool running = true;

	Piece board[8][8];
	int *board_binary;
	int board_binary_size;

	std::vector<int> gui_clients;

	engine() {
		board_binary_size = sizeof(int) * 8 * 8 * 2;
		board_binary = (int *)calloc(8 * 8 * 2, sizeof(int));
		init_board();
		board_to_binary(board, board_binary);
	}

	void init_board() {
		clear_board();

		for (int x = 0; x < 8; x++) {
			board[1][x] = Piece(PIECE_PAWN, WHITE);
			board[6][x] = Piece(PIECE_PAWN, BLACK);
		}

		board[0][0] = Piece(PIECE_ROOK, WHITE);
		board[0][7] = Piece(PIECE_ROOK, WHITE);
		board[0][2] = Piece(PIECE_BISHOP, WHITE);
		board[0][5] = Piece(PIECE_BISHOP, WHITE);
		board[0][1] = Piece(PIECE_KNIGHT, WHITE);
		board[0][6] = Piece(PIECE_KNIGHT, WHITE);
		board[0][3] = Piece(PIECE_QUEEN, WHITE);
		board[0][4] = Piece(PIECE_KING, WHITE);

		board[7][0] = Piece(PIECE_ROOK, BLACK);
		board[7][7] = Piece(PIECE_ROOK, BLACK);
		board[7][2] = Piece(PIECE_BISHOP, BLACK);
		board[7][5] = Piece(PIECE_BISHOP, BLACK);
		board[7][1] = Piece(PIECE_KNIGHT, BLACK);
		board[7][6] = Piece(PIECE_KNIGHT, BLACK);
		board[7][3] = Piece(PIECE_QUEEN, BLACK);
		board[7][4] = Piece(PIECE_KING, BLACK);
		player_turn = WHITE;
	}

	void clear_board() {
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				board[y][x] = Piece(PIECE_EMPTY, NONE);
			}
		}
	}

	void print_board() {
		printf("  ");
		repeat_char('_', 24 + 4);
		printf("\n");

		for (int y = 7; y >= 0; y--) {
			printf("%i | ", y + 1);
			for (int x = 0; x < 8; x++) {
				printf(" %c ", piece_codes[board[y][x].type]);
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

	Pos get_move(const char *msg) {
		Pos move;
		printf(msg);

		char x;
		int y;

		char buf[MOVE_BUF_LEN];
		fgets(buf, MOVE_BUF_LEN, stdin);
		sscanf(buf, "%c%i", &x, &y);

		move.x = x - 'a';
		move.y = y - 1;

		return move;
	}

	bool is_turn(Pos pos) {
		return board[pos.y][pos.x].color == player_turn;
	}

	bool is_opp(Pos pos) {
		return board[pos.y][pos.x].color != player_turn && !is_empty(pos);
	}

	bool is_empty(Pos pos) {
		return board[pos.y][pos.x].color == NONE;
	}

	void err(const char *msg, ...) {
		va_list args;
		va_start(args, msg);
		printf("ERROR: ");
		vprintf(msg, args);
		va_end(args);

		printf("\n");
	}

	void commit_move(Pos start, Pos end) {
		board[end.y][end.x] = board[start.y][start.x];
		board[start.y][start.x] = Piece();
	}

	bool validate_pawn(Pos start, Pos end) {
		bool same_col = start.x == end.x;
		bool one_up = end.y - start.y == pawn_dir[player_turn];
		bool two_up = end.y - start.y == 2 * pawn_dir[player_turn];

		bool one_side = abs(start.x - end.x) == 1;

		if (same_col && (one_up || two_up)) {
			Pos above = Pos(start.x, start.y + pawn_dir[player_turn]);
			if (!is_empty(end) || (two_up && !is_empty(above))) {
				err("piece in the way");
				return false;
			}

		} else if (one_side && one_up) {
			if (!is_opp(end)) {
				err("no piece to capture");
				return false;
			}
		} else {
			err("invalid destination");
			return false;
		}

		return true;
	}

	bool validate_bishop(Pos start, Pos end) {
		int dx = end.x - start.x;
		int dy = end.y - start.y;

		if (abs(dx) != abs(dy)) {
			err("move must be diagonal");
			return false;
		} else {
			int step_x = dx / abs(dx);
			int step_y = dy / abs(dy);

			Pos pos = Pos(start.x, start.y);
			while (pos.x != end.x && pos.y != end.y) {
				printf("check [%i, %i]\n", pos.x, pos.y);
				if (!is_empty(pos)) {
					err("piece in the way");
					return false;
				}

				pos.x += step_x;
				pos.y += step_y;
			}

			if (!is_empty(pos) && !is_opp(pos)) {
				err("cannot caputre own piece");
				return false;
			}
		}

		return true;
	}

	bool validate_move(Pos start, Pos end) {
		switch (board[start.y][start.x].type) {
		case PIECE_PAWN:
			return validate_pawn(start, end);
			break;

		case PIECE_BISHOP:
			return validate_bishop(start, end);
			break;

		default:
			break;
		}

		return false;
	}

	void make_move() {
		Pos start = get_move("start:");
		Pos end = get_move("end:");

		if (!start.in_board()) {
			return err("start not in board");
		}
		if (!end.in_board()) {
			return err("end not in board");
		}

		if (!is_turn(start)) {
			return err("starting square is not player %s", color_codes[player_turn]);
		}

		if (validate_move(start, end)) {
			commit_move(start, end);

			if (player_turn == WHITE) player_turn = BLACK;
			else player_turn = WHITE;
		}
	}

	void send_board_all() {
		board_to_binary(board, board_binary);

		printf("sending board to %i clients\n", gui_clients.size());
		for (size_t i = 0; i < gui_clients.size(); i++) {
			printf("sending board\n");
			send(gui_clients[i], board_binary, board_binary_size, 0);
		}
	}

	void create_server() {
		int server = socket(AF_INET, SOCK_STREAM, 0);
		fcntl(server, F_SETFL, O_NONBLOCK);
		sockaddr_in addr;

		addr.sin_port = htons(PORT);
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_family = AF_INET;

		bind(server, (sockaddr *)&addr, sizeof(addr));
		listen(server, 10);

		printf("waiting for connections and messages\n");
		while (true) {
			int client = accept(server, NULL, NULL);

			if (client < 0) {
				// error occured

				if (no_msg()) {
					// the error is only signifying that there is no packets to poll for
				}
			} else {
				// a connection is received
				gui_clients.push_back(client);
				printf("received connection, sending %i\n", board_binary_size);
				printf("%i connections total\n", gui_clients.size());
				send(client, board_binary, board_binary_size, 0);
			}

			for (size_t i = 0; i < gui_clients.size(); i++) {
				char buf[TCP_BUF_LEN] = {0};
				int res = recv(gui_clients[i], buf, TCP_BUF_LEN, 0);

				if (res == 0) {
					// no more active connections
				} else if (res < 0) {
					// error occured

					if (no_msg()) {
						// the error is only signifying that there is no packets to poll for
					}
				} else {
					// a response consissting of res bytes is received
					printf("received message \"%s\"\n", buf);
				}
			}
		}

		close(server);
	}

	// this creates a server that accepts connection requests
	void register_broadcast(int i) {
		create_server();
	}

	void run() {
		while (running) {
			print_board();
			make_move();
			send_board_all();
		}
	}
};

} // namespace chess