#CCFLAGS=-O2 -Wall
CCFLAGS=-Wall -g -DDEBUG -lgdbm -lpcre

all: src/utils.c src/syslog2stats.c
	gcc ${CGFLAGS} -c src/utils.c -o src/utils.o
	gcc ${CCFLAGS} src/syslog2stats.c src/utils.o -o src/syslog2stats

clean: src/syslog2stats
	rm src/syslog2stats srC/utils.o
