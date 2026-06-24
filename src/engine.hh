#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <vector>

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include "globals.hh"

namespace chess {

class client_t {
  public:
	int socket;
	COLOR color;

	client_t(int socket, COLOR color) {
		this->socket = socket;
		this->color = color;
	}
};

class engine {
  public:
	bool running = true;

	Board board;
	packet board_binary;

	char next_move[20];
	volatile bool has_move = false;

	std::vector<client_t> gui_clients;
	std::thread thread;
	Logger logger;

	engine() {
		printf("\n");
		logger.note("starting engine");
		if (!board.init()) {
			close_engine();
			return;
		}

		board.logger = &logger;
		board_to_packet(board, board_binary);
	}

	Move get_move(const char *msg) {
		while (!has_move) {
			// waiting
		}

		has_move = false;

		// char buf[MOVE_BUF_LEN];
		// printf(msg);
		// fgets(buf, MOVE_BUF_LEN, stdin);

		return Move(next_move);
	}

	void make_move(std::vector<Move> &moves) {
		Move move = get_move("move:");

		if (!move.start.in_board()) {
			return logger.err("start not in board");
		}
		if (!move.end.in_board()) {
			return logger.err("end not in board");
		}

		if (move.start.x == move.end.x && move.start.y == move.end.y) {
			return logger.err("end position identical to start position");
		}

		if (!board.is_turn(move.start)) {
			return logger.err("starting square is not player %s", color_codes[board.player_turn]);
		}

		if (!board.validate(moves, move)) {
			return logger.err("move is invalid");
		}
		board.commit(move);
		board.next_turn();
	}

	void send_board_all() {
		board_to_packet(board, board_binary);

		logger.net("sending board to %li clients", gui_clients.size());

		for (size_t i = 0; i < gui_clients.size(); i++) {
			board_binary.you = gui_clients[i].color;
			send(gui_clients[i].socket, (void *)&board_binary, sizeof(packet), 0);
		}

		board.captured = PIECE_EMPTY;
	}

	void create_listener() {
		const int opt = 1;
		int server = socket(AF_INET, SOCK_STREAM, 0);
		setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
		fcntl(server, F_SETFL, O_NONBLOCK);
		sockaddr_in addr;

		addr.sin_port = htons(PORT);
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_family = AF_INET;

		bind(server, (sockaddr *)&addr, sizeof(addr));
		int status = listen(server, 10);

		if (status < 0) {
			logger.err("failed to establish socket");
			running = false;
			return;
		}

		logger.note("waiting for connections");

		while (running) {
			int client = accept(server, NULL, NULL);
			fcntl(client, F_SETFL, O_NONBLOCK);

			if (client < 0) {
				// error occured

				if (no_msg()) {
					// the error is only signifying that there is no packets to poll for
				}
			} else {
				// a connection is received
				COLOR col = NONE;

				// the first two clients get a color assigned
				if (gui_clients.size() == 0) col = WHITE;
				else if (gui_clients.size() == 1) col = BLACK;
				gui_clients.push_back(client_t(client, col));
				logger.net("received connection (%i connections total)", gui_clients.size());

				board_binary.you = gui_clients[gui_clients.size() - 1].color;
				send(client, (void *)&board_binary, sizeof(packet), 0);
			}

			for (size_t i = 0; i < gui_clients.size(); i++) {
				char buf[TCP_BUF_LEN] = {0};
				int res = recv(gui_clients[i].socket, buf, TCP_BUF_LEN, 0);

				if (res == 0) {
					// inactive connection
					gui_clients.erase(gui_clients.begin() + i);
					i--;
				} else if (res < 0) {
					// error occured

					if (no_msg()) {
						// the error is only signifying that there is no packets to poll for
					}
				} else {
					// a response consissting of res bytes is received
					logger.net("received move \"%s\"", buf);
					strcpy(next_move, buf);
					has_move = true;
				}
			}
		}

		logger.net("closing server");
		close(server);
	}

	// this creates a server that accepts connection requests
	void create_server() {
		thread = std::thread(&chess::engine::create_listener, this);
	}

	void run() {
		while (running) {
			// board.draw();
			std::vector<Move> moves = board.get_valid_moves();
			make_move(moves);
			send_board_all();
		}
	}

	void close_engine() {
		running = false;
		thread.join();
	}
};

} // namespace chess