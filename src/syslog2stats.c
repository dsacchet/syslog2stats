#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <pcre.h>
#include "utils.h"
#include "syslog2stats.h"

#define SSIZE_MAX 32767

unsigned int reload_config=0;
unsigned int normal_termination=0;
unsigned int fatal_termination=0;
unsigned int flush_to_disk=0;


void signal_handler_update_config(int signo) {
	printf("signal %d (update config)\n",signo);
	reload_config=1;
}

void signal_handler_termination(int signo) {
	printf("signal %d (termination)\n",signo);
	switch(signo) {
	case SIGINT:
		normal_termination=1;
		break;
	case SIGTERM:
		fatal_termination=1;
		break;
	default:
		break;
	}
}

void signal_handler_flush_to_disk(int signo) {
	printf("signal %d (flush to disk)\n",signo);
	flush_to_disk=1;
}

/* Recherche une cle dans la liste chainee, si la cle est
 * retrouvee, on renvoie un pointeur sur le maillon en question
 * sinon on renvoi NULL */

pcrelist* pcrelist_search_name(pcrelist*pl,char*name) {
	while(pl->next != NULL) {
		if(strcmp(pl->name,name) == 0) {
			return pl;
		}
	}
	return NULL;
}

/* Destruction d'une liste de regexp */

int pcrelist_destroy(pcrelist**pl) {
	pcrelist*cur;
	pcrelist*next;
	if(*pl == NULL) {
		return 0;
	}
	cur = *pl;
	next = cur->next;
	while(next != NULL) {
		free(cur->name);
		free(cur->re);
		free(cur->pe);
		free(cur);
		cur=next;
		next=cur->next;
	}
	free(cur->name);
	free(cur->re);
	free(cur->pe);
	free(cur);
	return 0;
}

/* Insertion d'une regexp dans une liste (creation si vide) */

int pcrelist_insert(pcrelist**pl,const char*name, pcre*re, pcre_extra*pe) {
	pcrelist*pl_new;
	pcrelist*pl_cur=*pl;
	
	/* Creation du nouveau noued */
	pl_new=(pcrelist*)malloc(sizeof(pcrelist));
	if(pl_new == NULL) {
		return 1;
	}
	pl_new->name = (char*)malloc(sizeof(char)*(strlen(name)+1));
	memcpy(pl_new->name,name,strlen(name));
	pl_new->name[strlen(name)]='\0';
	pl_new->re=re;
	pl_new->pe=pe;
	pl_new->next=NULL;

	/* Insertion de celui ci � l'endroit voulu */
	if(*pl==NULL) {
		*pl=pl_new;
	} else {
		while(pl_cur->next != NULL) {
			pl_cur = pl_cur->next;
		}
		pl_cur->next = pl_new;
	}

	/* Ok, tout c'est bien pass� ! */
	return 0;
}

/* Lit la configuration du fichier passe en parametre et retourne une liste *
 * d'expressions regulieres validees */

pcrelist* read_config(const char * config_file) {

	int fd;
	ssize_t nb_char;
	unsigned int nb_regexp=0;
	bool_t need_to_read;
	char c;
	int i = 0;
	unsigned int nb_lines = 0;
	char*cle;
	char*valeur;

	pcrelist *first_pl = NULL;
	
	pcre *tmp_re;
	pcre_extra *tmp_pe;
	
	const char *pcre_errptr;
	int pcre_erroffset;
	

	/* Ouverture du fichier de configuration */
	fd=open(config_file,O_RDONLY);
	if(fd==-1) {
		printf("Can't open the configuration file\n");
		printf("(We try '%s')\n",config_file);
		return NULL;
	}

	/* Parsing du fichier de configuration */
	/* Une ligne par configuration */
	/* avant le : c'est la cl� */
	/* apr�s c'est l'expression r�guli�re */

	nb_char=0;
	nb_regexp=0;
	need_to_read=TRUE;
	while(read(fd,&c,1)) {

		/* On a trouv� le s�parateur entre la cl� et la regexp */
		/* Notez que la cl� ne peut donc pas contenir de : */
		if(c==':') {
			i=1;
		}

		/* Tant qu'on n'a pas trouv� la fin de ligne, on incr�mente */
		/* le nombre de caract�res lus */
		if(c!='\n') {
			nb_char++;
		}

		/* Si on a trouv� � la fois le caract�re de s�paration */
		/* et le caract�re de fin de ligne, OK, on peut traiter */
		if(i==1 && c =='\n') {

			nb_lines++;

			/* On revient en arri�re dans le fichier du */
			/* nombre de caract�res et on alloue un espace */
			/* m�moire de la bonne taille pour la lecture */
			/* du bloc */
			lseek(fd,-nb_char-1,SEEK_CUR);
			cle=malloc(nb_char+1);
			read(fd,cle,nb_char);

			/* On place ensuite les caract�res de fin de */
			/* chaine correctement pour la suite des */
			/* op�rations */
			cle[nb_char]='\0';
			valeur=index(cle,':')+1;
			cle[index(cle,':')-cle]='\0';

			/* On essaye de compiler la regexp fournie */
			/* Si il y a une erreur on rend la main ... */
			tmp_re=pcre_compile(valeur,0,&pcre_errptr,&pcre_erroffset,NULL);
			if(tmp_re == NULL) {
				printf("The regexp '%s' specified for key '%s' is not valid\n",valeur,cle);
				pcrelist_destroy(&first_pl);
				return NULL;
			/* Sinon on insere dans la liste chainee ... */
			} else {
				printf("The regexp '%s' specified for key '%s' is valid\n",valeur,cle);
				tmp_pe=pcre_study(tmp_re,0,&pcre_errptr);
				if(pcre_errptr == NULL) {
					printf("The study of the regexp '%s' specified for key '%s' was a success\n",valeur,cle);
				} else {
					printf("The study of the regexp '%s' specified for key '%s' failed\n",valeur,cle);
					pcrelist_destroy(&first_pl);
					return NULL;
				}
				pcrelist_insert(&first_pl,cle,tmp_re,tmp_pe);
			}
			nb_char=0;
			lseek(fd,1,SEEK_CUR);
			free(cle);
		}
		if(i==0 && c == '\n') {
			nb_lines++;
			printf("Line number %d doesn't hold a correct definition\n",nb_lines);
			nb_char=0;
		}
		
	}
	nb_char=0;
	
	return first_pl;
}

/* Realise des lectures de la taille maximale permise et dispatche dans *
 * le buffer en separant par le separateur defini dans le buffer */

int read_to_buffer(int fd,buffer_t*buffer) {
	ssize_t result;
	char*file_buffer;
	char*chunk_buffer;

	char*beginning_of_chunk;
	char*end_of_chunk;
	char*last_char_of_buffer;

	unsigned int chunk_size=0;
	unsigned int incomplete_size=0;


	/* In a first phase, we will get characters from the file */

	/* If we have a pending chunk in the buffer structure, first copy it
	 * at the beginning of the local buffer, and then read from network */

	if(buffer->incomplete_chunk != NULL) {
		incomplete_size=strlen(buffer->incomplete_chunk);
		file_buffer=malloc((SSIZE_MAX+incomplete_size)*sizeof(char));
		if(file_buffer == NULL) {
			errno=ENOMEM;
			return -1;
		}
		memcpy(file_buffer,buffer->incomplete_chunk,incomplete_size);
		result=ss_read(fd,file_buffer+incomplete_size,SSIZE_MAX);
		file_buffer[incomplete_size+result]='\0';
		free(buffer->incomplete_chunk);
		buffer->incomplete_chunk=NULL;

	/* Else only read from the network */

	} else {
		file_buffer=malloc(SSIZE_MAX*sizeof(char));
		if(file_buffer == NULL) {
			errno=ENOMEM;
			return -1;
		}
		result=ss_read(fd,file_buffer,SSIZE_MAX);
		file_buffer[result]='\0';
	}

	last_char_of_buffer=file_buffer+result*sizeof(char);

	/* Loop on the buffer to find chunk (elements separated by
	 * buffer->separator) */

	beginning_of_chunk=file_buffer;
	end_of_chunk=NULL;

	while((end_of_chunk=strstr(beginning_of_chunk,buffer->separator)) != NULL) {
		chunk_size=(end_of_chunk-beginning_of_chunk)/sizeof(char);
		chunk_buffer=malloc((chunk_size+1)*sizeof(char));
		if(chunk_buffer==NULL) {
			free(file_buffer);
			/* Try to backup the current read data */
			buffer->incomplete_chunk=strdup(beginning_of_chunk);
			file_buffer=NULL;
			errno=ENOMEM;
			return -1;
		}
		memcpy(chunk_buffer,beginning_of_chunk,chunk_size);
		chunk_buffer[chunk_size]='\0';
		buffer_push(buffer,chunk_buffer);
		beginning_of_chunk=end_of_chunk+strlen(buffer->separator)*sizeof(char);
		free(chunk_buffer);
		chunk_buffer=NULL;
	}

	/* If the buffer still contains some characters, just add them to
	 * incomplete_buffer */
	if(strlen(beginning_of_chunk) != 0) {
		buffer->incomplete_chunk=strdup(beginning_of_chunk);
	}

	free(file_buffer);
	file_buffer=NULL;

	return result;
}

/* Cette fonction positionne le pointeur a l'intertieur du file descriptor *
 * nb_lines avant la position courante. Une ligne est definie par le *
 * separateur specifie (\n le plus souvent) */

unsigned int rewind_of_n_lines(int fd, unsigned int nb_lines, char separator) {
	unsigned int result = 0;
	char cur_char[1];
	off_t cur_pos, end_pos;
	cur_pos=lseek(fd,0,SEEK_CUR);
	end_pos=lseek(fd,0,SEEK_END);
	cur_pos=lseek(fd,cur_pos,SEEK_SET);
	if(end_pos==cur_pos) {
		cur_pos=lseek(fd,-1,SEEK_CUR);
	}
	while(result != (nb_lines+1) && cur_pos != 0) {
		ss_read(fd,cur_char,1);
		if(cur_char[0] == separator) {
			result++;
		}
		cur_pos=lseek(fd,-2,SEEK_CUR);
	}
	if(cur_pos !=0) {
		lseek(fd,2,SEEK_CUR);
	}
	return result;
}

/* On commence le programme */

int main(
	int	argc,
	char	*argv[]
) {
	int nb_line_max=10;
	int fd;
	char*config_file=NULL;
	buffer_t*buffer;
	char*temp;
	bool_t daemonize = FALSE;

	char		*name;
	char		*file;
	extern char	*optarg;
	extern int	optind;
	extern int	opterr;
	extern int	optopt;
	char		c;
	char		**endconv;
	static const struct option	long_options[] = {
		{ "help",	no_argument,	NULL,	'h'},
		{ "lines",	required_argument,	NULL,	'n'},
		{ "daemon",	no_argument,	NULL,	'D'},
		{ "config-files",	required_argument,	NULL,	'c'}
	};
	
	pcrelist*pl;
	pcrelist*pl_tmp;

	int result;
	int ovector[30];

	/* On r�cup�re le nom du programme pour l'aide */

	name=rindex(argv[0],'/');
	if(name == NULL) {
		name=argv[0];
	} else {
		name++;
	}
	
	/* Traitement des options */
	
	opterr=0;
	endconv=(char**)malloc(sizeof(char*));

	while((c=getopt_long(argc,argv,"hDn:c:",long_options,NULL)) != -1) {
		switch(c) {
			case 'h':
				printf("Aide\n");
				return 1;
				break;
			case 'n':
				nb_line_max=(int)strtol(optarg,endconv,10);
				if(**endconv == '\0' && *optarg != '\0') {
				} else {
					printf("argument non valide\n");
					return 1;
				}
				break;
			case 'D':
				printf("Debug : coucou\n");
				if(daemonize==TRUE) {
					printf("Daemonize vaut FALSE\n");
					daemonize=FALSE;
				} else {
					printf("Daemonize vaut TRUE\n");
					daemonize=TRUE;
				}
				break;
			case 'c':
				config_file=malloc(strlen(optarg)+1);
				memcpy(config_file,optarg,strlen(optarg));
				config_file[strlen(optarg)]='\0';
				break;
			case '?':
			default:
				break;
		}
	}
	free(endconv);

	/* Si le fichier de configuration n'est pas sp�cifi� */
	/* on utilise par d�faut un fichier nomm� stats.conf */
	/* dans le r�pertoire courant */
	
	if(config_file==NULL) {
		config_file=malloc(strlen(DFT_CONFIG_FILE)+1);
		config_file=DFT_CONFIG_FILE;
	}
	
	/* Si il ne reste pas un argument, le fichier � traiter n'a */
	/* pas �t� sp�cifi� => erreur */

	if(argc-optind == 0) {
		printf("You need to specified a file to watch\n");
		return 1;
	}
	if(argc-optind > 1) {
		printf("You specified too many argument, we can watch only once file per run\n");
		return 1;
	}

	/* Le fichier � traiter est le prochain argument */
	
	file=(char*)argv[optind];

	pl=read_config(config_file);
	if(pl == NULL) {
		printf("Error during parsing of configuration file\n");
		return 1;
	}

	/* Ouverture du fichier et positionnement a la fin */
	fd=open(file,O_RDONLY);
	if(fd==-1) {
		printf("Cannot open the file '%s' to watch\n",file);
		return 1;
	}

	/* On se place � la fin du fichier, on retourne de nb_line_max en *
	 * on lit dans le buffer, et on commence la boucle */
	lseek(fd,0,SEEK_END);
	rewind_of_n_lines(fd,nb_line_max,'\n');
	buffer=buffer_init("\n",2048);
	read_to_buffer(fd,buffer);

	if(daemonize==TRUE) {
		printf("We will go on background ! Bye bye ...\n");
		daemon(0,0);
	}

	signal_ignore(SIGPIPE);
	signal_ignore(SIGUSR2);

	signal(SIGHUP,signal_handler_update_config);
	signal(SIGINT,signal_handler_termination);
	signal(SIGTERM,signal_handler_termination);
	signal(SIGUSR1,signal_handler_flush_to_disk);

	while(1) {
		if(normal_termination || fatal_termination) {
			buffer_destroy(&buffer);
			close(fd);
			pcrelist_destroy(&pl);
			free(config_file);
			exit(0);
		}
		/* On lit dans le fichier */
		read_to_buffer(fd,buffer);

		/* Si on au minimum un chunk, on le traite */
		if(buffer->nb_chunk != 0) {
			/* On compare ce chunk � toutes les regexp qu'on a */
			while((temp=buffer_shift(buffer,FALSE))!= NULL) {
				pl_tmp=pl;
				while(pl_tmp != NULL) {
					result=pcre_exec(pl_tmp->re,pl_tmp->pe,
							temp,strlen(temp),
							0, 0, ovector, 30);
					if(result>0) {
						printf("Regexp %s match on string '%s'\n",pl_tmp->name,temp);
					}
					if(result<-1) {
						printf("Regexp %s dont match on string '%s' and raise error %d\n",pl_tmp->name,temp,result);
					}
					pl_tmp=pl_tmp->next;
				}
				free(temp);
			}
		/* Sinon c'est qu'il n'y a pas beaucoup d'activit� donc on *
		 * attend un peu */
		} else {
			sleep(1);
		}
	}

	return 0;
}
