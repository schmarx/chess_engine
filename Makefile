FMT_ESC = \033[
FMT_END = m

FMT_RESET = ${FMT_ESC}0${FMT_END}
FMT_RED = ${FMT_ESC}31${FMT_END}
FMT_GREEN = ${FMT_ESC}32${FMT_END}
FMT_YELLOW = ${FMT_ESC}33${FMT_END}
FMT_BLUE = ${FMT_ESC}34${FMT_END}

SRC_DIR = src
OBJ_DIR = bin

SRC = src/main.cpp

output_file = bin/main

links = -lm

FLAGS = -Wall -pedantic -O3

OBJ = ${subst src,bin,${SRC:.cpp=.o}}

run: ${OBJ}
	@printf "${FMT_GREEN}linking${FMT_RESET}\n"
	@g++ ${FLAGS} ${OBJ} -o ${output_file} ${links}
	${output_file} ${N} ${w} ${h}

clean:
	rm -r ./bin

refresh: clean run


${OBJ_DIR}/%.o: ${SRC_DIR}/%.cpp
	@printf "> making    ${FMT_YELLOW}$@${FMT_RESET}"
	@mkdir -p ${dir $@}
	@g++ ${FLAGS} $^ -c -o $@
	@printf "\33[2k\r"
	@printf "> made      ${FMT_BLUE}$@${FMT_RESET}\n"


count:
	@wc -l $$(git ls-files | grep 'src/.*')
	@echo ""
	@echo commits:
	@git rev-list --count --all