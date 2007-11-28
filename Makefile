#CCFLAGS=-O2 -Wall
CCFLAGS=-Wall -g -DDEBUG -lgdbm -lpcre

all: src/syslog2stats.c
	gcc ${CCFLAGS} src/syslog2stats.c -o src/syslog2stats

clean: src/syslog2stats
	rm src/syslog2stats
