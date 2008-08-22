#ifndef _UTILS_H_
#define _UTILS_H_

#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include "error.h"
#include <time.h>


#ifndef MAX
#define MAX(a, b) (a > b ? a : b)
#endif

#ifndef MIN
#define MIN(a, b) (a < b ? a : b)
#endif

#define MAXNBTIMERS 512

typedef enum
{ FALSE, TRUE } bool_t;

typedef struct buffer_chunk {
	char*buffer;
	ssize_t nb_chars;
	struct buffer_chunk*prev;
	struct buffer_chunk*next;
	
} buffer_chunk_t;

typedef enum
{ UNUSED, INUSE } timer_status_t;

typedef enum
{ RESET, GET } timer_action_t;

typedef struct buffer {
	char*separator;
	unsigned int nb_chunk;
	unsigned int nb_chunk_max;
	unsigned long long discarded;
	unsigned long long added;
	unsigned long long deleted;
	char*incomplete_chunk;
	pthread_mutex_t mutex;
	buffer_chunk_t*first;
	buffer_chunk_t*last;
} buffer_t;

char* buffer_shift(buffer_t*,bool_t);
int buffer_push(buffer_t*,const char*);
int buffer_push_vprintf(buffer_t*,const char*, va_list);
int buffer_push_printf(buffer_t*,const char*, ...);
int buffer_push_vnprintf(buffer_t*,size_t, const char*, va_list);
int buffer_push_nprintf(buffer_t*,size_t, const char*, ...);
buffer_t* buffer_init(const char*,unsigned int);
int buffer_destroy(buffer_t**);
void signal_ignore(int);
void signal_reset(int);
unsigned int ss_sleep(unsigned int);
ssize_t ss_write(int, const void*, size_t);
ssize_t ss_read(int, void*, size_t);

struct timeval timers(timer_action_t, unsigned short int);

#endif

