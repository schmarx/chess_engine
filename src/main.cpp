#include <stdio.h>
#include <stdlib.h>
#include <thread>

#include "engine.hh"

int main(int argc, char *argv[]) {
	chess::engine eng = chess::engine();

	std::thread th = std::thread(&chess::engine::register_broadcast, &eng, 1);
	eng.run();

	return EXIT_SUCCESS;
}