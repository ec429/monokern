#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <SDL.h>
#include <SDL_image.h>

#include "kern.h"

#define min(a,b)	((a)<(b)?(a):(b))
#define max(a,b)	((a)>(b)?(a):(b))

SDL_Surface *ginit(unsigned int w, unsigned int h, unsigned char bpp);
void pchar(SDL_Surface *scrn, unsigned int x, unsigned int y, char c);
void pstr(SDL_Surface *scrn, unsigned int x, unsigned int y, const char *s);
void dpstr(SDL_Surface *scrn, unsigned int x, unsigned int y, const char *s, const signed char *dev, bool iscy, unsigned int cx);
void invert(SDL_Surface *scrn, SDL_Rect r);

void init_char(char **buf, int *l, int *i);
void append_char(char **buf, int *l, int *i, char c);
char * fgetl(FILE *fp);

SDL_Surface *letters[96];

typedef struct
{
	unsigned int y;
	unsigned int x;
}
point;

typedef struct
{
	unsigned int nlines; // scrollback size
	unsigned int rows; // screen size
	unsigned int cols;
	char **text;
	signed char **dev;
	bool (*dirty)[2]; // [0]=dev, [1]=screen
	point cur;
	point old;
	unsigned int esc;
	char escd[256]; // escape codes buffer
}
terminal;

int initterm(terminal *t, unsigned int nlines, unsigned int rows, unsigned int cols);
void cdown(terminal *t);
void cright(terminal *t, bool wrap);
int getpm(const char *escd, unsigned int *i, unsigned int *g, unsigned int e);

int main(int argc, char *argv[])
{
	const char *program="sh";
	char *fake_arg=NULL, *const *argp=&fake_arg;
	if(argc>1)
	{
		if((strcmp(argv[1], "--help")==0)||(strcmp(argv[1], "-h")==0))
		{
			fprintf(stderr, "Usage: termk [<program> [--args <args> [...]]]\n");
		}
		else
		{
			program=argv[1];
			argp=argv+1;
		}
	}
	
	SDL_Surface *screen=ginit(500, 320, 32);
	if(!screen)
	{
		fprintf(stderr, "termk: ginit failed: %s\n", SDL_GetError());
		return(EXIT_FAILURE);
	}
	for(int i=0;i<96;i++)
	{
		char lfn[14];
		sprintf(lfn, "as/as_%hhu.pbm", i+32);
		if(!(letters[i]=IMG_Load(lfn)))
		{
			fprintf(stderr, "termk: IMG_Load failed: %s\n", IMG_GetError());
			return(EXIT_FAILURE);
		}
	}
	FILE *kf=fopen("scores", "r");
	KERN *k=kern_init(kf);
	if(!k)
	{
		fprintf(stderr, "termk: kern_init failed\n");
		return(EXIT_FAILURE);
	}
	fclose(kf);
	
	terminal t;
	if(initterm(&t, 24, 24, 80)) return(EXIT_FAILURE);
	
	int ptmx=open("/dev/ptmx", O_RDWR);
	if(!ptmx)
	{
		perror("open");
		return(EXIT_FAILURE);
	}
	if(grantpt(ptmx))
	{
		perror("grantpt");
		return(EXIT_FAILURE);
	}
	if(unlockpt(ptmx))
	{
		perror("unlockpt");
		return(EXIT_FAILURE);
	}
	
	pid_t pid=fork();
	switch(pid)
	{
		case -1: // error
			perror("fork");
			return(EXIT_FAILURE);
		break;
		case 0: // child
		setsid();
		setenv("TERM", "vt52", 1);
		char *pts=ptsname(ptmx);
		if(!pts)
		{
			perror("ptsname");
			return(EXIT_FAILURE);
		}
		int ptsfd=open(pts, O_RDWR);
		if(!ptsfd)
		{
			perror("open");
			return(EXIT_FAILURE);
		}
		if(dup2(ptsfd, STDIN_FILENO)==-1)
		{
			perror("dup2 in");
			return(EXIT_FAILURE);
		}
		if(dup2(ptsfd, STDOUT_FILENO)==-1)
		{
			perror("dup2 out");
			return(EXIT_FAILURE);
		}
		if(dup2(ptsfd, STDERR_FILENO)==-1)
		{
			perror("dup2 err");
			return(EXIT_FAILURE);
		}
		execvp(program, argp);
		// if we get here, execvp failed
		perror("execvp");
		return(EXIT_FAILURE);
		break;
		default: // parent
		break;
	}
	
	fd_set master, readfds;
	FD_ZERO(&master);
	FD_SET(ptmx, &master);
	int fdmax=ptmx;
	struct timeval tv;
	
	SDL_FillRect(screen, &(SDL_Rect){0, 0, 500, 320}, SDL_MapRGB(screen->format, 0, 0, 0));
	SDL_Flip(screen);
	SDL_EnableUNICODE(1);
	SDL_Event event;
	int errupt=0;
	while(!errupt)
	{
		readfds=master;
		tv.tv_sec=tv.tv_usec=0;
		if(select(fdmax+1, &readfds, NULL, NULL, &tv)==-1)
		{
			if(errno!=EINTR) // nobody cares if select() was interrupted by a signal
				perror("select");
		}
		else
		{
			if(FD_ISSET(ptmx, &readfds))
			{
				char c;
				ssize_t b=read(ptmx, &c, 1);
				if(b==0)
				{
					fprintf(stderr, "termk: EOF, unexpectedly (program exit)\n");
					kill(pid, SIGKILL);
					return(EXIT_SUCCESS);
				}
				else if(b<0)
				{
					perror("read");
					kill(pid, SIGKILL);
					return(EXIT_FAILURE);
				}
				else
				{
					if(c==0x7f)
					{
						c=0;
					}
					else if((signed char)c<0)
					{
						c=0x7f;
					}
					if(t.esc)
					{
						if((c==0x18)||(c==0x1a)) // CAN, SUB cancel escapes
						{
							fprintf(stderr, "termk: cancelled ESC:");
							for(unsigned int i=0;i<t.esc;i++)
								fprintf(stderr, " %02x", t.escd[i]);
							fputc('\n', stderr);
							t.esc=0;
							c=0x7f; // replacement character
							goto do_print;
						}
						else if(t.esc<256)
						{
							t.escd[t.esc++]=c;
							if(t.esc==2)
							{
								switch(c)
								{
									case 'A':
										if(t.cur.y) t.cur.y--;
										t.esc=0;
									break;
									case 'B':
										cdown(&t);
										t.esc=0;
									break;
									case 'C':
										cright(&t, false);
										t.esc=0;
									break;
									case 'D':
										if(t.cur.x) t.cur.x--;
										t.esc=0;
									break;
									case 'H':
										t.cur.x=t.cur.y=0;
										t.esc=0;
									break;
									case 'I': // reverse line feed
										for(unsigned int i=t.nlines-1;i>0;i--)
										{
											memcpy(t.text[i], t.text[i-1], t.cols);
											t.dirty[i][1]=true;
										}
										t.dirty[0][0]=false;
										t.dirty[0][1]=true;
										memset(t.text[0], ' ', t.cols);
										memset(t.dev[0], 0, t.cols);
										t.esc=0;
									break;
									case 'J':
									{
										unsigned int y=t.cur.y,x=t.cur.x;
										while(y<t.rows)
										{
											while(x<t.cols)
												t.text[y][x++]=' ';
											x=0;
											t.dirty[y++][0]=true;
										}
									}
										t.esc=0;
									break;
									case 'K':
									{
										unsigned int x=t.cur.x;
										while(x<t.cols)
											t.text[t.cur.y][x++]=' ';
										t.dirty[t.cur.y][0]=true;
									}
										t.esc=0;
									break;
									case 'Y': // cursor move (takes 2 more bytes)
										// nothing
									break;
									case '[':
										// suck it up, it's not a vt52 sequence but the sending application doesn't know what it's doing
									break;
									default:
										fprintf(stderr, "termk: unknown ESC:");
										for(unsigned int i=0;i<t.esc;i++)
											fprintf(stderr, " %02x", t.escd[i]);
										fputc('\n', stderr);
										t.esc=0;
										c=0x7f; // replacement character
										goto do_print;
									break;
								}
							}
							else
							{
								switch(t.escd[1])
								{
									case 'Y': // cursor move (takes 2 more bytes)
										if(t.esc==4)
										{
											// ESC Y Ps Ps, cursor move
											t.cur.y=min(max(t.escd[2]-32, 0), t.rows-1);
											t.cur.x=min(max(t.escd[3]-32, 0), t.cols-1);
											t.esc=0;
										}
									break;
									case '[': // broken non-vt52 sequence ^[[m sent by some errant programs
										if(c=='m')
											t.esc=0;
									break;
								}
							}
						}
					}
					else if(c<0x20)
					{
						switch(c)
						{
							case 7: // BEL
								//bell()
							break;
							case 8: // BS
								if(t.cur.x) t.cur.x--;
							break;
							case 9: // HT
								cright(&t, false);
								while(t.cur.x&7) cright(&t, false);
							break;
							case 0xa: // LF
							case 0xb: // VT
							case 0xc: // FF
								t.cur.x=0;
								cdown(&t);
							break;
							case 0xd: // CR
								t.cur.x=0;
							break;
							case 0x1b:
								t.esc=1;
								t.escd[0]=c;
							break;
						}
					}
					else
					{
						do_print:
						t.text[t.cur.y][t.cur.x]=c;
						t.dirty[t.cur.y][0]=true;
						cright(&t, false);
					}
				}
			}
			else
			{
				for(unsigned int i=0;i<t.rows;i++)
				{
					if(t.dirty[i][0])
					{
						kern(t.text[t.nlines+i-t.rows], t.dev[t.nlines+i-t.rows], k);
						t.dirty[i][0]=false;
						t.dirty[i][1]=true;
					}
					if(t.dirty[i][1]||(i==t.cur.y)||(i==t.old.y))
					{
						SDL_FillRect(screen, &(SDL_Rect){0, 4+i*13, 500, 13}, SDL_MapRGB(screen->format, 0, 0, 0));
						dpstr(screen, 4, 4+i*13, t.text[t.nlines+i-t.rows], t.dev[t.nlines+i-t.rows], i==t.cur.y, t.cur.x);
						t.dirty[i][1]=false;
					}
				}
				SDL_Flip(screen);
				t.old=t.cur;
			}
			for(int fd=0;fd<=fdmax;fd++)
			{
				if(FD_ISSET(fd, &readfds))
					FD_CLR(fd, &readfds);
			}
		}
		if(SDL_PollEvent(&event))
		{
			switch(event.type)
			{
				case SDL_QUIT:
					kill(pid, SIGKILL);
					return(EXIT_SUCCESS);
				break;
				case SDL_KEYDOWN:
					if(event.key.type==SDL_KEYDOWN)
					{
						SDL_keysym key=event.key.keysym;
						if(key.sym==SDLK_UP)
						{
							ssize_t b=write(ptmx, "\033[A", 3);
							if(b<3)
								perror("write");
						}
						else if(key.sym==SDLK_DOWN)
						{
							ssize_t b=write(ptmx, "\033[B", 3);
							if(b<3)
								perror("write");
						}
						else if(key.sym==SDLK_RIGHT)
						{
							ssize_t b=write(ptmx, "\033[C", 3);
							if(b<3)
								perror("write");
						}
						else if(key.sym==SDLK_LEFT)
						{
							ssize_t b=write(ptmx, "\033[D", 3);
							if(b<3)
								perror("write");
						}
						else if((key.unicode&0xFF80)==0)
						{
							char k=key.unicode&0x7F;
							if(k)
							{
								ssize_t b=write(ptmx, &k, 1);
								if(b<0)
									perror("write");
							}
						}
					}
				break;
				/*case SDL_MOUSEMOTION:
					mousex=event.motion.x;
					mousey=event.motion.y;
				break;
				case SDL_MOUSEBUTTONDOWN:
					mousex=event.button.x;
					mousey=event.button.y;
				break;*/
			}
		}
	}
	return(0);
}

int initterm(terminal *t, unsigned int nlines, unsigned int rows, unsigned int cols)
{
	if(!t)
	{
		fprintf(stderr, "initterm: t==NULL\n");
		return(1);
	}
	t->nlines=nlines;
	t->rows=rows;
	t->cols=cols;
	t->cur.x=0;
	t->cur.y=nlines-rows;
	t->esc=0;
	t->text=malloc(nlines*sizeof(char *));
	if(!t->text)
	{
		perror("initterm: malloc");
		return(-1);
	}
	t->dev=malloc(nlines*sizeof(signed char *));
	if(!t->dev)
	{
		free(t->text);
		perror("initterm: malloc");
		return(-1);
	}
	t->dirty=malloc(nlines*sizeof(bool[2]));
	if(!t->dirty)
	{
		free(t->text);
		free(t->dev);
		perror("initterm: malloc");
		return(-1);
	}
	for(unsigned int i=0;i<nlines;i++)
	{
		t->dirty[i][0]=false;
		t->dirty[i][1]=true;
		t->text[i]=malloc(cols);
		if(!t->text[i])
		{
			perror("initterm: malloc");
			for(;i>0;)
				free(t->text[--i]);
			free(t->text);
			free(t->dev);
			free(t->dirty);
			return(2);
		}
		t->dev[i]=malloc(cols);
		if(!t->dev[i])
		{
			perror("initterm: malloc");
			for(;i>0;)
			{
				free(t->text[i]);
				free(t->dev[--i]);
			}
			free(t->text[0]);
			free(t->text);
			free(t->dev);
			free(t->dirty);
			return(2);
		}
		for(unsigned int j=0;j<cols;j++)
		{
			t->text[i][j]=' ';
			t->dev[i][j]=0;
		}
	}
	return(0);
}

void cdown(terminal *t)
{
	if(++t->cur.y>=t->nlines)
	{
		for(unsigned int i=0;i<t->nlines-1;i++)
		{
			memcpy(t->text[i], t->text[i+1], t->cols);
			memcpy(t->dev[i], t->dev[i+1], t->cols);
			t->dirty[i][0]=t->dirty[i+1][0];
			t->dirty[i][1]=true;
		}
		t->cur.y=t->nlines-1;
		t->dirty[t->cur.y][0]=false;
		t->dirty[t->cur.y][1]=true;
		memset(t->text[t->cur.y], ' ', t->cols);
		memset(t->dev[t->cur.y], 0, t->cols);
	}
}

void cright(terminal *t, bool wrap)
{
	if(++t->cur.x>=t->cols)
	{
		if(wrap)
		{
			t->cur.x=0;
			cdown(t);
		}
		else
			t->cur.x--;
	}
}

int getpm(const char *escd, unsigned int *i, unsigned int *g, unsigned int e)
{
	unsigned int s=*i;
	while(*i<e)
	{
		if(escd[*i]==';')
			break;
		else if(!isdigit(escd[*i]))
			break;
		(*i)++;
	}
	if(*i==s) return(0);
	char d[*i+1-s];
	memcpy(d, escd+s, *i-s);
	d[*i-s]=0;
	return(sscanf(d, "%u", g));
}

SDL_Surface *ginit(unsigned int w, unsigned int h, unsigned char bpp)
{
	SDL_Surface *rv=NULL;
	if(SDL_Init(SDL_INIT_VIDEO)<0)
	{
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return(NULL);
	}
	if((rv=SDL_SetVideoMode(w, h, bpp, SDL_HWSURFACE))==NULL)
	{
		fprintf(stderr, "SDL_SetVideoMode: %s\n", SDL_GetError());
		SDL_Quit();
		return(NULL);
	}
	atexit(SDL_Quit);
	return(rv);
}

void pchar(SDL_Surface *scrn, unsigned int x, unsigned int y, char c)
{
	if((signed char)c>=32)
		SDL_BlitSurface(letters[(unsigned char)c-32], NULL, scrn, &(SDL_Rect){x, y, 0, 0});
}

void pstr(SDL_Surface *scrn, unsigned int x, unsigned int y, const char *s)
{
	if(!s) return;
	while(*s)
	{
		pchar(scrn, x, y, *s++);
		x+=6;
	}
}

void dpstr(SDL_Surface *scrn, unsigned int x, unsigned int y, const char *s, const signed char *dev, bool iscy, unsigned int cx)
{
	if(!s) return;
	unsigned int scx=0;
	while(*s)
	{
		pchar(scrn, x+*dev, y, *s++);
		if(iscy&&(scx==cx))
			invert(scrn, (SDL_Rect){x+*dev, y, 5, 12});
		dev++;
		x+=6;
		scx++;
	}
}

void invert(SDL_Surface *scrn, SDL_Rect r)
{
	for(int x=r.x;x<r.x+r.w;x++)
		for(int y=r.y;y<r.y+r.h;y++)
		{
			long int s_off = (y*scrn->pitch) + x*scrn->format->BytesPerPixel;
			unsigned char *pixloc = ((unsigned char *)scrn->pixels)+s_off;
			pixloc[0]^=0xFF;
			pixloc[1]^=0xFF;
			pixloc[2]^=0xFF;
		}	
}

char * fgetl(FILE *fp)
{
	char * lout;
	int l,i;
	init_char(&lout, &l, &i);
	signed int c;
	while(!feof(fp))
	{
		c=fgetc(fp);
		if((c==EOF)||(c=='\n'))
			break;
		if(c=='\t')
		{
			append_char(&lout, &l, &i, ' ');
			while(i&3)
				append_char(&lout, &l, &i, ' ');
		}
		else if(c!=0)
		{
			append_char(&lout, &l, &i, c);
		}
	}
	return(lout);
}

void append_char(char **buf, int *l, int *i, char c)
{
	if(!((c==0)||(c==EOF)))
	{
		if(*buf)
		{
			(*buf)[(*i)++]=c;
		}
		else
		{
			init_char(buf, l, i);
			append_char(buf, l, i, c);
		}
		char *nbuf=*buf;
		if((*i)>=(*l))
		{
			*l=*i*2;
			nbuf=(char *)realloc(*buf, *l);
		}
		if(nbuf)
		{
			*buf=nbuf;
			(*buf)[*i]=0;
		}
		else
		{
			free(*buf);
			init_char(buf, l, i);
		}
	}
}

void init_char(char **buf, int *l, int *i)
{
	*l=80;
	*buf=(char *)malloc(*l);
	(*buf)[0]=0;
	*i=0;
}
