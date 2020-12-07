CC = gcc

all:
	${CC} -o lab1 lab1.c -ldl
	${CC} -o parity.so parity.c -shared -fPIC
