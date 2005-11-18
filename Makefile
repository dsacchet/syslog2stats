#CCFLAGS=-O2 -Wall
CCFLAGS=-Wall -g -DDEBUG -ldb -lpcre

all: src/syslog2stats.c
	gcc ${CCFLAGS} src/syslog2stats.c -o src/syslog2stats
