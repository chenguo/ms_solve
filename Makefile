all: ms_solve.c
	gcc ms_solve.c -o ms_solve

clean:
	@rm -f ms_solve