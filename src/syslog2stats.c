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
#include <db.h>
#include <pcre.h>

#define	BUFSIZE	8

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif
				       
#define DFT_CONFIG_FILE "stats.conf"
				       
int main(
	int	argc,
	char	*argv[]
) {


	int fd;
	char buffer[BUFSIZE+1];
	off_t end_pos=0;
	off_t start_pos=0;
	off_t cur_pos=0;
	off_t first_pos_to_print=0;
	ssize_t nb_char;
	ssize_t nb_char_read;
	int nb_line_max=10;
	int nb_line=0;
	int i;
	int need_to_read;
	char*config_file=NULL;

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
		{ "config-files",	required_argument,	NULL,	'c'}
	};
	
	struct linebuffer {
		char buffer[BUFSIZE+1];
		size_t nb_char;
		struct linebuffer *next;
	};

	struct linebuffer *first_ln;
	struct linebuffer *cur_ln;
	struct linebuffer *tmp_ln;

	struct pcrelist {
		char *name;
		pcre *re;
		pcre_extra *pe;
		struct pcrelist *next;
	};

	struct pcrelist *first_pl;
	struct pcrelist *cur_pl;
	struct pcrelist *tmp_pl;
	
	pcre *tmp_re;
	pcre_extra *tmp_pe;
	
	char *pcre_errptr;
	int pcre_erroffset;
	
	char*newline;
	char*cle;
	char*valeur;
	int nb_regexp=0;
	char*buffer_variable;
	int ovector[30];

	/* On récupére le nom du programme pour l'aide */

	name=rindex(argv[0],'/');
	if(name == NULL) {
		name=argv[0];
	} else {
		name++;
	}
	
	/* Traitement des options */
	
	opterr=0;
	endconv=(char**)malloc(sizeof(char*));

	while((c=getopt_long(argc,argv,"hn:c:",long_options,NULL)) != -1) {
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

	/* Si le fichier de configuration n'est pas spécifié */
	/* on utilise par défaut un fichier nommé stats.conf */
	/* dans le répertoire courant */
	
	if(config_file==NULL) {
		config_file=malloc(strlen(DFT_CONFIG_FILE)+1);
		config_file=DFT_CONFIG_FILE;
	}
	
	/* Si il ne reste pas un argument, le fichier à  traiter n'a */
	/* pas été spécifié => erreur */

	if(argc-optind != 1) {
		printf("Pas de fichier\n");
		return 1;
	}

	/* Le fichier à traiter est le prochaine argument */
	
	file=(char*)argv[optind];

	/* Ouverture du fichier de configuration */
	fd=open(config_file,O_RDONLY);
	if(fd==-1) {
		printf("Erreur à l'ouverture du fichier de configuration\n");
		return 1;
	}

	/* Parsing du fichier de configuration */
	/* Une ligne par configuration */
	/* avant le : c'est la clé */
	/* après c'est l'expression régulière */

	nb_char=0;
	nb_regexp=0;
	need_to_read=1;
	while(read(fd,&c,1)) {

		/* On a trouvé le séparateur entre la clé et la regexp */
		/* Notez que la clé ne peut donc pas contenir de : */
		if(c==':') {
			i=1;
		}

		/* Tant qu'on n'a pas trouvé la fin de ligne, on incrémente */
		/* le nombre de caractères lus */
		if(c!='\n') {
			nb_char++;
		}

		/* Si on a trouvé à la fois le caractère de séparation */
		/* et le caractère de fin de ligne, OK, on peut traiter */
		if(i==1 && c =='\n') {

			/* On revient en arrière dans le fichier du */
			/* nombre de caractères et on alloue un espace */
			/* mémoire de la bonne taille pour la lecture */
			/* du bloc */
			lseek(fd,-nb_char-1,SEEK_CUR);
			cle=malloc(nb_char+1);
			read(fd,cle,nb_char);

			/* On place ensuite les caractères de fin de */
			/* chaine correctement pour la suite des */
			/* opérations */
			cle[nb_char]='\0';
			valeur=index(cle,':')+1;
			cle[index(cle,':')-cle]='\0';

			tmp_re=pcre_compile(valeur,0,&pcre_errptr,&pcre_erroffset,NULL);
			if(tmp_re == NULL) {
				printf("Erreur sur la regexp (%s) de la clÃ© %s\n",valeur,cle);
			} else {
				printf("OK sur la regexp (%s) de la clÃ© %s\n",valeur,cle);
				nb_regexp++;
				if(nb_regexp==1) {
					first_pl=cur_pl=tmp_pl=malloc(sizeof(struct pcrelist));
				} else {
					tmp_pl=malloc(sizeof(struct pcrelist));
					cur_pl->next=tmp_pl;
					cur_pl=tmp_pl;
				}
				cur_pl->name=cle;
				cur_pl->re=tmp_re;
				cur_pl->pe=NULL;
				cur_pl->next=NULL;

				tmp_pe=pcre_study(tmp_re,0,&pcre_errptr);
				if(pcre_errptr == NULL) {
					printf("OK sur l'étude de la regexp (%s) de la clé %s\n",valeur,cle);
					cur_pl->pe=tmp_pe;
				} else {
					printf("Erreur sur l'étude de la regexp (%s) de la clé %s\n",valeur,cle);
				}
				cur_pl=tmp_pl;
			}
			nb_char=0;
			lseek(fd,1,SEEK_CUR);
		}
		if(i==0 && c == '\n') {
			nb_char=0;
		}
		
	}
	nb_char=0;
	
	tmp_pl=first_pl;
	while(tmp_pl != NULL) {
			printf("On a une regexp pour la clé %s (re: %d pe: %d)\n",tmp_pl->name,tmp_pl->re,tmp_pl->pe);
			tmp_pl=tmp_pl->next;
	}
	
	/* Ouverture du fichier et positionnement Ã  la fin */
	fd=open(file,O_RDONLY);
	if(fd==-1) {
		printf("Erreur à l'ouverture du fichier\n");
		return 1;
	}
	end_pos=lseek(fd,0,SEEK_END);

	/* On lit le dernier chunk dont la taille est infÃ©rieure Ã  */
	/* BUFSIZE afin d'avoir un nombre entier de chunk pour la */
	/* boucle suivante */
	
	cur_pos=end_pos/BUFSIZE*BUFSIZE;
	cur_pos=lseek(fd,cur_pos,SEEK_SET);
	nb_char=read(fd,buffer,(ssize_t)(end_pos-cur_pos));
	cur_pos=lseek(fd,cur_pos,SEEK_SET);
	buffer[nb_char]='\0';

	/* On analyse ce chunk pour la prÃ©sence de caractÃ¨res de */
	/* retour Ã  la ligne */

	for(i=(int)nb_char-1;i>=0;i--) {
		if(buffer[i] == '\n') {
			if(nb_line==nb_line_max) {
				first_pos_to_print=cur_pos+(off_t)i+1;
				nb_line++;
				break;
			}
			nb_line++;
		}
	}
	
	/* On parcourt le reste du fichier Ã  l'envers jusqu'Ã  */
	/* atteindre le début du fichier ou bien le nombre de */
	/* lignes désiré */
	
	while(first_pos_to_print == 0 &&
			cur_pos != start_pos &&
			nb_line != nb_line_max+1) {
		cur_pos=lseek(fd,-BUFSIZE,SEEK_CUR);
		nb_char=read(fd,buffer,(ssize_t)(BUFSIZE));
		cur_pos=lseek(fd,-BUFSIZE,SEEK_CUR);
		for(i=(int)nb_char-1;i>=0;i--) {
			if(buffer[i] == '\n') {
				if(nb_line==nb_line_max) {
					first_pos_to_print=cur_pos+(off_t)i+1;
					nb_line++;
					break;
				}
				nb_line++;
			}
		}
	}

	/* On commence le traitement à partir de first_pos_to_print */
	
	cur_pos=lseek(fd,first_pos_to_print,SEEK_SET);
	
	/* On initialise la liste chainée des buffers et les variables */
	/* de controle de la boucle */
	
	first_ln=cur_ln=malloc(sizeof(struct linebuffer));
	first_ln->nb_char=0;
	first_ln->next=NULL;
	newline=NULL;
	need_to_read=1;

	int result;
	DB *db_stats=NULL;
	result=db_create(&db_stats,NULL,0);
	if(result != 0) {
		printf("Erreur à la création de l'environnement DB.\n");
		return 1;
	}
	result=db_stats->open(db_stats,NULL,"stats.db",NULL,DB_HASH,DB_CREATE,0);
	if(result != 0) {
		printf("Erreur à l'ouverture de DB.\n");
		return 1;
	}
	db_stats->close(db_stats,0);
	
	while(1) {

		/* Si le traitement de la précédente lecture est fini */
		/* (plus de caractère de retour à la ligne, alors on */
		/* lit dans le fichier, si jamais la lecture retourne */
		/* 0 caractères, on dort un peu avant de réessayer */
		
		if(need_to_read) {
			nb_char_read=read(fd,buffer,BUFSIZE);
			buffer[nb_char_read]='\0';
			if(nb_char_read==0) {
				sleep(1);
				continue;
			}
		}

		/* On cherche un caractère de retour à la ligne */
		newline=index(buffer,'\n');

		/* Si on en trouve un, on affiche le contenu de la ligne */
		/* (pour l'instant, par la suite, on fera le traitement */
		/* avec les regexp et la mise à jour d'une base de données */
		/* de type berkeley attaquable en SNMP ...) */
		
		if(newline != NULL) {

			/* Il faut déjà compter le nombre de caractères */
			/* présents dans le buffer liste chainée afin  */
			/* de mallocer assez d'espace pour l'affichage */
			/* ou traitement de la ligne */
			
			tmp_ln=first_ln;
			nb_char=0;
			while(tmp_ln->next!=NULL) {
				nb_char+=tmp_ln->nb_char;
				tmp_ln=tmp_ln->next;
			}
			nb_char+=tmp_ln->nb_char+newline-buffer;

			/* Création de la chaine de caractère */
			
			buffer_variable=(char*)malloc(sizeof(char)*nb_char+1);

			/* Recopie des petits bouts dans la chaine finale */
			
			tmp_ln=first_ln;
			nb_char=0;
			while(tmp_ln->next!=NULL) {
				memcpy(buffer_variable+nb_char,tmp_ln->buffer,tmp_ln->nb_char);
				nb_char+=tmp_ln->nb_char;
				tmp_ln=tmp_ln->next;
			}
			memcpy(buffer_variable+nb_char,tmp_ln->buffer,tmp_ln->nb_char);
			nb_char+=tmp_ln->nb_char;
			memcpy(buffer_variable+nb_char,buffer,newline-buffer);
			nb_char+=newline-buffer;
			buffer_variable[nb_char]='\0';

			/* buffer_variable contient ce qu'il faut, maintenant */
			/* on l'affiche, plus tard on effectuera un */
			/* traitement dessus */
			
			tmp_pl=first_pl;
			while(tmp_pl != NULL) {
				result=pcre_exec(tmp_pl->re,tmp_pl->pe,
						buffer_variable,nb_char,
						0, 0, ovector, 30);
				if(result>0) {
					printf("Regexp %s match on string '%s'\n",tmp_pl->name,buffer_variable);
				}
				if(result<-1) {
					printf("Regexp %s dont match on string '%s' and raise error %d\n",tmp_pl->name,buffer_variable,result);
				}
				tmp_pl=tmp_pl->next;
			}
			free(buffer_variable);

			/* On libère la liste chainée pour recommencer avec */
			/* une vide */
			
			cur_ln=tmp_ln=first_ln;
			while(cur_ln->next!=NULL) {
				cur_ln=cur_ln->next;
				free(tmp_ln);
				tmp_ln=cur_ln;
			}
			cur_ln->nb_char=0;
			first_ln=cur_ln;

			/* On met à jour le buffer de lecture et on dit */
			/* qu'on a encore des caractères à traiter dans */
			/* celui-ci */
			
			memcpy(buffer,newline+1,nb_char_read+(ssize_t)buffer-(ssize_t)newline-1);
			buffer[nb_char_read+(ssize_t)buffer-(ssize_t)newline-1]='\0';
			nb_char_read=nb_char_read+(ssize_t)buffer-(ssize_t)newline-1;
			need_to_read=0;
		} else {

			/* On regarde si le nombre de caractère du buffer */
			/* rentre dans l'espace restant dans le chunk actuel */
			
			/* Si oui, on copie dedans */
			
			if(nb_char_read<=BUFSIZE-cur_ln->nb_char) {
				memcpy(cur_ln->buffer+cur_ln->nb_char,buffer,nb_char_read);
				cur_ln->nb_char+=nb_char_read;

			/* Si non, on coupe en deux et on colle dans l'actuel */
			/* et dans un nouveau */
				
			} else {
				/* Copy du premier bout dans le buffer courant */
				memcpy(cur_ln->buffer+cur_ln->nb_char,buffer,BUFSIZE-cur_ln->nb_char);
				/* Création d'un nouveau buffer */
				tmp_ln=malloc(sizeof(struct linebuffer));
				tmp_ln->nb_char=0;
				tmp_ln->next=NULL;
				/* Copie de l'autre bout dans le nouveau buffer */
				memcpy(tmp_ln->buffer,buffer+BUFSIZE-cur_ln->nb_char,cur_ln->nb_char);
				/* Mise à jour des compteurs */
				tmp_ln->nb_char=cur_ln->nb_char;
				cur_ln->nb_char=BUFSIZE;
				/* On fait pointer cur_ln sur le nouveau buffer */
				cur_ln->next=tmp_ln;
				cur_ln=tmp_ln;
			}
			need_to_read=1;
		}
	}

	return 0;
	
}
