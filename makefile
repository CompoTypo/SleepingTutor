build:
	gcc -g -O2 csmc.c -pthread -o csmc

builder:
	gcc -g -O2 csmc2.c -pthread -o csmc

test1:
	./csmc 10 3 4 5

test2:
	./csmc 2000 10 20 4


