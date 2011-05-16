all: ms_solve.c
	gcc ms_solve.c -o ms_solve -lpthread

debug: ms_solve.c
	gcc ms_solve.c -g -ggdb -o ms_solve -lpthread


clean:
	@rm -f ms_solve