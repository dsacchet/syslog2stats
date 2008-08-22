#include "utils.h"

/* This function get back the first chunk of the buffer, and delete
 * it from it. Moreover, it updates all counters and pointers inside
 * the structure.
 * This function is thread safe. The same buffer can be manipulated
 * by several thread using this function */

char* buffer_shift(buffer_t*buffer,bool_t add_separator) {
	char*return_value;
	buffer_chunk_t*bc_temp;
	ssize_t separator_len;
	ssize_t chunk_len;

	/* lock the buffer */
	pthread_mutex_lock(&(buffer->mutex));

	/* If the chunk buffer list is empty, set errno, free the mutex and
	 * exit with an error */
	if(buffer->first == NULL) {
		pthread_mutex_unlock(&(buffer->mutex));
		errno=ENODATA;
		return NULL;
	}

	/* Get the buffer */
	if(add_separator) {
		return_value=malloc((buffer->first->nb_chars+strlen(buffer->separator)+1)*sizeof(char));
	} else {
		return_value=malloc((buffer->first->nb_chars+1)*sizeof(char));
	}
	if(return_value == NULL) {
		pthread_mutex_unlock(&(buffer->mutex));
		errno=ENOMEM;
		return NULL;
		
	}
	chunk_len=buffer->first->nb_chars;
	memcpy(return_value,buffer->first->buffer,chunk_len);
	if(add_separator) {
		separator_len=strlen(buffer->separator);
		memcpy(return_value+chunk_len,buffer->separator,separator_len);
		return_value[chunk_len+separator_len]='\0';
	} else {
		return_value[chunk_len]='\0';
	}

	/* If the list has only one chunk, set last directly to NULL */

	if(buffer->first == buffer->last) {
		buffer->last = NULL;
	}

	/* Free space used by the chunk */
	free(buffer->first->buffer);
	buffer->first->buffer=NULL;
	bc_temp=buffer->first;
	buffer->first=buffer->first->next;
	free(bc_temp);
	bc_temp=NULL;

	/* Update stats */
	buffer->nb_chunk--;
	buffer->deleted++;

	/* Free mutex and return no error */
	pthread_mutex_unlock(&(buffer->mutex));
	return return_value;
}


/* This function add a chunk at the end of the buffer. If the maximum
 * number of chunks is already present, an error is raised.
 * This function is thread safe. The same buffer can be manipulated
 * by several thread using this function */

int buffer_push(buffer_t*buffer,const char*chunk_buffer) {
	buffer_chunk_t*bc_new;

	/* Lock the buffer */
	pthread_mutex_lock(&(buffer->mutex));

	/* If the buffer is full, discard the chunk, set errno
	 * free the mutex and exit with an error */
	if(buffer->nb_chunk == buffer->nb_chunk_max && buffer->nb_chunk_max != 0) {
		pthread_mutex_unlock(&(buffer->mutex));
		buffer->discarded++;
		errno=ENOBUFS;
		return -1;
	}

	/* Create the new chunk */
	bc_new=malloc(sizeof(buffer_chunk_t));
	if(bc_new == NULL) {
		pthread_mutex_unlock(&(buffer->mutex));
		buffer->discarded++;
		errno=ENOMEM;
		return -1;
	}
	bc_new->buffer=strdup(chunk_buffer);
	bc_new->nb_chars=strlen(bc_new->buffer);
	bc_new->next=NULL;

	/* Chain the chunk at the end of the buffer_chunk list */
	bc_new->prev=buffer->last;
	if(buffer->last != NULL) {
		buffer->last->next=bc_new;
	}
	buffer->last=bc_new;
	if(buffer->first == NULL) {
		buffer->first = buffer->last;
	}

	/* Update stats */
	buffer->nb_chunk++;
	buffer->added++;

	/* Free mutex and return no error */
	pthread_mutex_unlock(&(buffer->mutex));
	return 0;
}

/* This function add a chunk which could be formatted with printf. A string
 * of the exact size is created on demand. Then we use buffer_push function
 * to effectively add this chunk to the buffer. */

int buffer_push_vprintf(buffer_t*buffer,const char*format, va_list va) {
	char*chunk_buffer;
	int nb_chars;
	int result;

	/* Make a first try to get the final string size */
	nb_chars=vsnprintf(chunk_buffer,0,format,va);

	/* Try to allocate the memory */
	chunk_buffer=malloc((nb_chars+1)*sizeof(char));
	if(chunk_buffer == NULL) {
		errno=ENOMEM;
		return -1;
	}

	/* Effectively create the string */
	vsprintf(chunk_buffer,format,va);

	/* Try yo add it to the buffer */
	result = buffer_push(buffer,chunk_buffer);
	free(chunk_buffer);
	chunk_buffer=NULL;
	if(result == -1) {
		return -1;
	}

	/* If succcess, return the number of chars */
	return nb_chars;
}

/* This function add a chunk which could be formatted with printf. A string
 * of the exact size is created on demand. Then we use buffer_push function
 * to effectively add this chunk to the buffer. */

int buffer_push_printf(buffer_t*buffer,const char*format, ...) {
	char*chunk_buffer;
	va_list va;
	int nb_chars;
	int result;

	/* Make a first try to get the final string size */
	va_start(va,format);
	nb_chars=vsnprintf(chunk_buffer,0,format,va);
	va_end(va);

	/* Try to allocate the memory */
	chunk_buffer=malloc((nb_chars+1)*sizeof(char));
	if(chunk_buffer == NULL) {
		errno=ENOMEM;
		return -1;
	}

	/* Effectively create the string */
	va_start(va,format);
	vsprintf(chunk_buffer,format,va);
	va_end(va);

	/* Try yo add it to the buffer */
	result = buffer_push(buffer,chunk_buffer);
	free(chunk_buffer);
	chunk_buffer=NULL;
	if(result == -1) {
		return -1;
	}

	/* If succcess, return the number of chars */
	return nb_chars;
}

/* This function add a chunk which could be formatted with printf. A string
 * of the exact size is created on demand. We limit the maximum size with
 * a parameter. Then we use buffer_push function to effectively add this chunk
 * to the buffer. */

int buffer_push_vnprintf(buffer_t*buffer,size_t size, const char*format, va_list va) {
	char*chunk_buffer;
	int nb_chars;
	int result;

	/* Make a first try to get the final string size */
	nb_chars=vsnprintf(chunk_buffer,0,format,va);

	/* If the final size is greater than the wanted size, raise an error */
	if((nb_chars-1) > (int)size) {
		errno=ENAMETOOLONG;
		return -1;
	}

	/* Try to allocate the memory */
	chunk_buffer=malloc((nb_chars+1)*sizeof(char));
	if(chunk_buffer == NULL) {
		errno=ENOMEM;
		return -1;
	}

	/* Effectively create the string */
	vsprintf(chunk_buffer,format,va);

	/* Try yo add it to the buffer */
	result = buffer_push(buffer,chunk_buffer);
	free(chunk_buffer);
	chunk_buffer=NULL;
	if(result == -1) {
		return -1;
	}

	/* If succcess, return the number of chars */
	return nb_chars;
}

/* This function add a chunk which could be formatted with printf. A string
 * of the exact size is created on demand. We limit the maximum size with
 * a parameter. Then we use buffer_push function to effectively add this chunk
 * to the buffer. */

int buffer_push_nprintf(buffer_t*buffer,size_t size, const char*format, ...) {
	char*chunk_buffer;
	va_list va;
	int nb_chars;
	int result;

	/* Make a first try to get the final string size */
	va_start(va,format);
	nb_chars=vsnprintf(chunk_buffer,0,format,va);
	va_end(va);

	/* If the final size is greater than the wanted size, raise an error */
	if((nb_chars-1) > (int)size) {
		errno=ENAMETOOLONG;
		return -1;
	}

	/* Try to allocate the memory */
	chunk_buffer=malloc((nb_chars+1)*sizeof(char));
	if(chunk_buffer == NULL) {
		errno=ENOMEM;
		return -1;
	}

	/* Effectively create the string */
	va_start(va,format);
	vsprintf(chunk_buffer,format,va);
	va_end(va);

	/* Try yo add it to the buffer */
	result = buffer_push(buffer,chunk_buffer);
	free(chunk_buffer);
	chunk_buffer=NULL;
	if(result == -1) {
		return -1;
	}

	/* If succcess, return the number of chars */
	return nb_chars;
}

/* This function creates and initialize a new buffer. */

buffer_t* buffer_init(const char*separator,unsigned int nb_chunk_max) {
	buffer_t*buffer;
	buffer=malloc(sizeof(buffer_t));
	if(buffer == NULL) {
		errno=ENOMEM;
		return NULL;
	}
	buffer->separator=strdup(separator);
	buffer->nb_chunk=0;
	buffer->discarded=0;
	buffer->added=0;
	buffer->deleted=0;
	buffer->nb_chunk_max=nb_chunk_max;
	buffer->incomplete_chunk=NULL;
	buffer->first=NULL;
	buffer->last=NULL;
	return buffer;
}

/* This function frees memory used by a buffer */

int buffer_destroy(buffer_t**buffer) {
	buffer_chunk_t*bc_temp;

	if(*buffer == NULL) {
		return 0;
	}

	/* First free every chunk */
	bc_temp=(**buffer).first;
	while(bc_temp != NULL) {
		free(bc_temp->buffer);
		bc_temp->buffer=NULL;

		/* If we are on the last chunk, just free it */
		if(bc_temp->next == NULL) {
			free(bc_temp);
			bc_temp=NULL;

		/* ... else go to next chunk and free the prev one */
		} else {
			bc_temp=bc_temp->next;
			free(bc_temp->prev);
			bc_temp->prev=NULL;
		}
	}

	if((**buffer).incomplete_chunk != NULL) {
		free((**buffer).incomplete_chunk);
		(**buffer).incomplete_chunk=NULL;
	}

	if((**buffer).separator != NULL) {
		free((**buffer).separator);
		(**buffer).separator=NULL;
	}

	/* and finally free the buffer */
	free(*buffer);
	*buffer=NULL;
	return 0;
}

void signal_ignore(int signo) {
	signal(signo,SIG_IGN);
}

void signal_reset(int signo) {
	signal(signo,SIG_DFL);
}

/* The ss (for signal safe) functions are identifical to their counterpart
 * normal fonction, except they are able to handle correctly an interruption
 * by a signal handler */

/* When sleep() is interrupted by a signal handler, it returns the number
 * of seconds remaining to sleep, we loop until this value is 0 */

unsigned int ss_sleep(unsigned int seconds) {
	unsigned int result;
	result = sleep(seconds);
	while(result != 0) {
		result = sleep(result);
	}
	return 0;
}

/* If read() or write() are interrupted by a signal, they returns -1 and
 * set errno to EINTR, we loop until errno is not EINTR (we don't manage
 * other type of errors */

ssize_t ss_write(int fildes, const void *buf, size_t nbyte) {
	ssize_t result;
	result = write(fildes,buf,nbyte);
	while(result == -1 && errno == EINTR) {
		printf("Ecriture interrompue, on reprend\n");
		result = write(fildes,buf,nbyte);
	}
	return result;
}

ssize_t ss_read(int fildes, void *buf, size_t nbyte) {
	ssize_t result;
	result = read(fildes,buf,nbyte);
	while(result == -1 && errno == EINTR) {
		printf("Lecture interrompue, on reprend\n");
		result = read(fildes,buf,nbyte);
	}
	return result;
}

/* Allows to manage a set of timers to time different part of a program *
 * This function makes usage of static variables */

struct timeval timers(timer_action_t action, unsigned short int index) {
	static struct timeval timers[MAXNBTIMERS];
	static timer_status_t timers_status[MAXNBTIMERS];
	static bool_t first_time = TRUE;
	unsigned short int i;
	struct timeval tv;
	struct timeval result;
	result.tv_sec=0;
	result.tv_usec=0;

	/* The first time, we need to initialize the status */
	if(first_time == TRUE) {
		for(i=0;i<MAXNBTIMERS;i++) {
			timers_status[i]=UNUSED;
		}
		first_time = FALSE;
	}

	/* If index is out of bound, return 0 */
	if(index > MAXNBTIMERS-1) {
		errno=EINVAL;
		return result;
	}

	/* Perform action, either RESET or GET
	 *  - Reset will set the timers[index] to the current timestamp
	 *  - Get will return the diff between current timestamp and stored */
	switch(action) {
		case RESET:
			gettimeofday(&timers[index],NULL);
			timers_status[index]=INUSE;
			result.tv_sec=timers[index].tv_sec;
			result.tv_usec=timers[index].tv_usec;
			break;
		case GET:
			if(timers_status[index] == UNUSED) {
				result.tv_sec=0;
				result.tv_usec=0;
			} else {
				gettimeofday(&tv,NULL);
				if(tv.tv_usec < timers[index].tv_usec) {
					result.tv_usec=tv.tv_usec+1000000-timers[index].tv_usec;
					i=1;
				} else {
					result.tv_usec=tv.tv_usec-timers[index].tv_usec;
					i=0;
				}
				result.tv_sec=tv.tv_sec-timers[index].tv_sec-i;
			}
			break;
	}
	return result;
}
