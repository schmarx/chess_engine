#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "engine.hh"

chess::engine eng;

void handle_interrupt(int signal) {
	printf("\n");
	eng.close_engine();

	exit(0);
}

int main(int argc, char *argv[]) {
	signal(SIGINT, handle_interrupt);

	eng.create_server();
	eng.run();

	return EXIT_SUCCESS;
}