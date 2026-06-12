#include <stdio.h>
#include <stdlib.h>

#include "engine.hh"

int main(int argc, char *argv[]) {
	engine eng = engine();
    eng.run();

	return EXIT_SUCCESS;
}