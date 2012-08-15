#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <term.h>
#include <signal.h>
#include <errno.h>
#include <SDL.h>
#include <X11/Xlib.h> // needed to ring the Xbell

#include "bits.h"
#include "kern.h"
#include "kfa.h"
#include "pbm.h"

#define SINCE_LIMIT	65536	// limit for 'batching' input before doing a screen update

#define min(a,b)	((a)<(b)?(a):(b))
#define max(a,b)	((a)>(b)?(a):(b))

typedef struct
{
	unsigned char fore, back;
	bool revvid;
	bool bold;
}
attr;

#define DEFAULT_ATTR ((attr){.fore=7, .back=0, .revvid=false, .bold=false})

typedef unsigned char colour[3];

SDL_Surface *ginit(unsigned int w, unsigned int h, unsigned char bpp);
void pchar(SDL_Surface *scrn, unsigned int x, unsigned int y, char c, attr a);
void dpstr(SDL_Surface *scrn, unsigned int x, unsigned int y, const char *s, const attr *at, const signed char *dev, bool iscy, unsigned int cx, bool forcebw);
void invert(SDL_Surface *scrn, SDL_Rect r);
void filter(SDL_Surface *scrn, SDL_Rect r, colour fore, colour back);
void colourmap(attr a, colour fore, colour back);

void do_write(int fd, const char *);

typedef struct
{
	SDL_Surface *data;
	SDL_Surface *bold;
	unsigned char what[3];
	signed char spa[2];
}
lig;

typedef struct
{
	char *name;
	bool bold;
	SDL_Surface *data;
}
lid;

SDL_Surface *letters[96];
SDL_Surface *bold[96];
SDL_Surface *metas[32]; // from M-_ to M-~
unsigned int nlids;
lid *lids;
unsigned int nligs;
lig *ligs;

typedef struct
{
	unsigned int y;
	unsigned int x;
}
point;

point fsiz={13, 6};

typedef struct
{
	unsigned int nlines; // scrollback size
	unsigned int rows; // screen size
	unsigned int cols;
	unsigned int scroll; // amount view is scrolled by
	unsigned int scrold;
	attr sgr; // currently set attributes
	char **text;
	attr **at;
	signed char **dev;
	bool (*dirty)[2]; // [0]=dev, [1]=screen
	point cur;
	point old;
	point save;
	bool meta;
	bool status;
	unsigned int statusp;
	char statusline[81]; // status-line buffer
	unsigned int esc;
	char escd[256]; // escape codes buffer
}
terminal;

int initterm(terminal *t, unsigned int nlines, unsigned int rows, unsigned int cols);
void cdown(terminal *t);
bool cright(terminal *t, bool wrap);
int getpm(const char *escd, unsigned int *i, unsigned int *g, unsigned int e);

int main(int argc, char *argv[])
{
	const char *program="bash";
	char *fake_arg=NULL, *const *argp=&fake_arg;
	const char *font="as";
	bool green=false;
	bool terminfo=true;
	bool wide=false;
	for(int arg=1;arg<argc;arg++)
	{
		if(argv[arg][0]=='-')
		{
			if(strcmp(argv[arg], "--")==0)
			{
				program=argv[arg+1];
				argp=argv+arg+1;
				break;
			}
			else if((strcmp(argv[arg], "--help")==0)||(strcmp(argv[arg], "-h")==0))
			{
				fprintf(stderr, "Usage: termk [<opts>] [<program> [<args>]]\n");
				fprintf(stderr, "Options:\n\t--\t\tEnd option list (for <program> beginning with '-')\n\t--green\t\tUse green display filter\n\t-18\t\tUse font \"18\"\n");
				return(EXIT_SUCCESS);
			}
			else if(strcmp(argv[arg], "--green")==0)
			{
				green=true;
			}
			else if(strcmp(argv[arg], "-18")==0)
			{
				font="18";
			}
			else if(strcmp(argv[arg], "--vt52")==0)
			{
				terminfo=false;
			}
			else if(strcmp(argv[arg], "--wide")==0)
			{
				wide=true;
			}
			else
			{
				fprintf(stderr, "termk: unrecognised option, ignoring: %s\n", argv[arg]);
			}
		}
		else
		{
			program=argv[arg];
			argp=argv+arg;
			break;
		}
	}
	
	const char *term=NULL;
	if(terminfo)
	{
		const char * const terms[]={"termk52-w", "termk52", "termk-vt52","termk","vt52-am",NULL};
		unsigned int i=wide?0:1;
		while(terms[i])
		{
			int errret;
			if(setupterm(terms[i], 1, &errret))
			{
				switch(errret)
				{
					case 1: // hardcopy
						// merely unsuitable, so carry on looking
						fprintf(stderr, "terminfo '%s' unsuitable: is hardcopy\n", terms[i]);
					break;
					case 0: // terminfo entry not found
						// merely unsuitable, so carry on looking
						fprintf(stderr, "terminfo '%s' not found\n", terms[i]);
					break;
					case -1: // terminfo database not found
					default: // unknown error
						// fatal error
						fprintf(stderr, "terminfo '%s': setupterm error %d\n", terms[i], errret);
						terminfo=false;
						goto noti;
					break;
				}
			}
			else
			{
				term=terms[i];
				fprintf(stderr, "terminfo '%s' selected\n", term);
				break;
			}
			i++;
		}
	}
	else
	{
		noti:
		term="vt52";
		fprintf(stderr, "no terminfo.  $TERM=%s\n", term);
	}
	
	KERN *k=NULL;
	for(unsigned int i=0;i<96;i++)
		letters[i]=NULL;
	for(unsigned int i=0;i<96;i++)
		bold[i]=NULL;
	for(unsigned int i=0;i<32;i++)
		metas[i]=NULL;
	nlids=0;
	lids=NULL;
	string ligatures=null_string();
	string kfar=make_string(PREFIX"/share/fonts/");
	append_str(&kfar, font);
	append_str(&kfar, ".termkf");
	FILE *kfa=fopen(kfar.buf, "r");
	if(!kfa)
	{
		fprintf(stderr, "termk: fopen: %s.termkf: %s\n", font, strerror(errno));
		return(EXIT_FAILURE);
	}
	kf_archive kfb;
	if(kf_read(kfa, &kfb)<0)
	{
		fprintf(stderr, "termk: kf_read failed\n");
		return(EXIT_FAILURE);
	}
	fclose(kfa);
	for(unsigned int i=0;i<kfb.nents;i++)
	{
		unsigned char j;
		if(sscanf(kfb.ents[i].name.buf, "as_%hhu.pbm", &j)==1)
		{
			if((j>=31)&&(j<128))
			{
				if(!(letters[j-32]=pbm_string(kfb.ents[i].data)))
				{
					fprintf(stderr, "termk: bad as/%s\n", kfb.ents[i].name.buf);
					return(EXIT_FAILURE);
				}
			}
			else
			{
				fprintf(stderr, "termk: bad as/%s\n", kfb.ents[i].name.buf);
				return(EXIT_FAILURE);
			}
		}
		else if(sscanf(kfb.ents[i].name.buf, "bo_%hhu.pbm", &j)==1)
		{
			if((j>=31)&&(j<128))
			{
				if(!(bold[j-32]=pbm_string(kfb.ents[i].data)))
				{
					fprintf(stderr, "termk: bad bo/%s\n", kfb.ents[i].name.buf);
					return(EXIT_FAILURE);
				}
			}
			else
			{
				fprintf(stderr, "termk: bad bo/%s\n", kfb.ents[i].name.buf);
				return(EXIT_FAILURE);
			}
		}
		else if(sscanf(kfb.ents[i].name.buf, "ma_%hhu.pbm", &j)==1)
		{
			if((j>=96)&&(j<128))
			{
				if(!(metas[j-96]=pbm_string(kfb.ents[i].data)))
				{
					fprintf(stderr, "termk: bad ma/%s\n", kfb.ents[i].name.buf);
					return(EXIT_FAILURE);
				}
			}
			else
			{
				fprintf(stderr, "termk: bad ma/%s\n", kfb.ents[i].name.buf);
				return(EXIT_FAILURE);
			}
		}
		else if(strncmp(kfb.ents[i].name.buf, "li_", 3)==0)
		{
			char *dot=strrchr(kfb.ents[i].name.buf, '.');
			if(dot)
			{
				lid l;
				l.bold=false;
				*dot=0;
				l.name=strdup(kfb.ents[i].name.buf+3);
				*dot='.';
				if(!(l.data=pbm_string(kfb.ents[i].data)))
				{
					free(l.name);
					fprintf(stderr, "termk: bad li/%s\n", kfb.ents[i].name.buf);
					return(EXIT_FAILURE);
				}
				unsigned int n=nlids++;
				lid *nl;
				if(!(nl=realloc(lids, nlids*sizeof(lid))))
				{
					free(l.name);
					free(l.data);
					perror("termk: realloc");
					return(EXIT_FAILURE);
				}
				(lids=nl)[n]=l;
			}
			else
			{
				fprintf(stderr, "termk: bad li/%s\n", kfb.ents[i].name.buf);
				return(EXIT_FAILURE);
			}
		}
		else if(strncmp(kfb.ents[i].name.buf, "lb_", 3)==0)
		{
			char *dot=strrchr(kfb.ents[i].name.buf, '.');
			if(dot)
			{
				lid l;
				l.bold=true;
				*dot=0;
				l.name=strdup(kfb.ents[i].name.buf+3);
				*dot='.';
				if(!(l.data=pbm_string(kfb.ents[i].data)))
				{
					free(l.name);
					fprintf(stderr, "termk: bad li/%s\n", kfb.ents[i].name.buf);
					return(EXIT_FAILURE);
				}
				unsigned int n=nlids++;
				lid *nl;
				if(!(nl=realloc(lids, nlids*sizeof(lid))))
				{
					free(l.name);
					free(l.data);
					perror("termk: realloc");
					return(EXIT_FAILURE);
				}
				(lids=nl)[n]=l;
			}
			else
			{
				fprintf(stderr, "termk: bad li/%s\n", kfb.ents[i].name.buf);
				return(EXIT_FAILURE);
			}
		}
		else if(strcmp(kfb.ents[i].name.buf, "ligatures")==0)
		{
			ligatures=dup_string(kfb.ents[i].data);
		}
		else if(strcmp(kfb.ents[i].name.buf, "scores")==0)
		{
			k=kern_init_s(kfb.ents[i].data, true);
		}
	}
	kf_free(kfb);
	
	nligs=0;
	ligs=NULL;
	if(ligatures.i)
	{
		unsigned int i=0;
		while(i<ligatures.i)
		{
			lig l;
			l.what[0]=ligatures.buf[i++];
			if(i==ligatures.i)
			{
				fprintf(stderr, "termk: bad ligatures file\n");
				return(EXIT_FAILURE);
			}
			l.spa[0]=ligatures.buf[i++];
			if(l.spa[0]=='-') l.spa[0]=-1;
			else if(l.spa[0]=='.') l.spa[0]=0;
			else if(l.spa[0]=='+') l.spa[0]=1;
			if(i==ligatures.i)
			{
				fprintf(stderr, "termk: bad ligatures file\n");
				return(EXIT_FAILURE);
			}
			l.what[1]=ligatures.buf[i++];
			if(i==ligatures.i)
			{
				fprintf(stderr, "termk: bad ligatures file\n");
				return(EXIT_FAILURE);
			}
			l.spa[1]=ligatures.buf[i++];
			if(l.spa[1]=='-') l.spa[1]=-1;
			else if(l.spa[1]=='.') l.spa[1]=0;
			else if(l.spa[1]=='+') l.spa[1]=1;
			if(i==ligatures.i)
			{
				fprintf(stderr, "termk: bad ligatures file\n");
				return(EXIT_FAILURE);
			}
			l.what[2]=ligatures.buf[i++];
			if(i==ligatures.i)
			{
				fprintf(stderr, "termk: bad ligatures file\n");
				return(EXIT_FAILURE);
			}
			if(ligatures.buf[i++]!=' ')
			{
				if((ligatures.buf[i-1]=='\n')&&(l.what[1]>=32)&&(l.what[1]<128))
				{
					(l.data=letters[l.what[1]-32])->refcount++;
					unsigned int n=nligs++;
					lig *nl=realloc(ligs, nligs*sizeof(lig));
					if(!nl)
					{
						SDL_FreeSurface(l.data);
						perror("termk: realloc");
						return(EXIT_FAILURE);
					}
					(ligs=nl)[n]=l;
					continue;
				}
				else
				{
					fprintf(stderr, "termk: bad ligatures file\n");
					return(EXIT_FAILURE);
				}
			}
			if(i==ligatures.i)
			{
				fprintf(stderr, "termk: bad ligatures file\n");
				return(EXIT_FAILURE);
			}
			unsigned int tl=strcspn(ligatures.buf+i, "\n"), j;
			for(j=0;j<nlids;j++)
			{
				if(lids[j].bold) continue;
				if(strlen(lids[j].name)!=tl) continue;
				if(strncmp(ligatures.buf+i, lids[j].name, tl)==0) break;
			}
			if(j<nlids)
			{
				(l.data=lids[j].data)->refcount++;
			}
			else
			{
				fprintf(stderr, "termk: ligatures: %.*s not found\n", tl, ligatures.buf+i);
				return(EXIT_FAILURE);
			}
			for(j=0;j<nlids;j++)
			{
				if(!lids[j].bold) continue;
				if(strlen(lids[j].name)!=tl) continue;
				if(strncmp(ligatures.buf+i, lids[j].name, tl)==0) break;
			}
			if(j<nlids)
				(l.bold=lids[j].data)->refcount++;
			else
				l.bold=NULL;
			i+=tl;
			unsigned int n=nligs++;
			lig *nl=realloc(ligs, nligs*sizeof(lig));
			if(!nl)
			{
				SDL_FreeSurface(l.data);
				perror("termk: realloc");
				return(EXIT_FAILURE);
			}
			(ligs=nl)[n]=l;
			if(i<ligatures.i) i++;
		}
	}
	free_string(&ligatures);
	
	for(unsigned int i=0;i<nlids;i++)
	{
		free(lids[i].name);
		SDL_FreeSurface(lids[i].data);
	}
	
	if(!k)
	{
		fprintf(stderr, "termk: kern_init failed\n");
		return(EXIT_FAILURE);
	}
	
	if(letters[0])
	{
		fsiz.y=letters[0]->h+1;
		fsiz.x=letters[0]->w+1;
	}
	
	terminal t;
	if(initterm(&t, 256, 24, wide?132:80)) return(EXIT_FAILURE);
	
	SDL_Surface *screen=ginit(8+t.cols*fsiz.x, 4+t.rows*fsiz.y, 32);
	if(!screen)
	{
		fprintf(stderr, "termk: ginit failed: %s\n", SDL_GetError());
		return(EXIT_FAILURE);
	}
	
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
		setenv("TERM", term, 1);
		char val[16];
		snprintf(val, 16, "%u", t.rows);
		setenv("LINES", val, 1);
		snprintf(val, 16, "%u", t.cols);
		setenv("COLUMNS", val, 1);
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
	
	Display *dpy = XOpenDisplay(NULL);
	if(!dpy)
		fprintf(stderr, "termk: failed to open X display, no ^G bell\n");
	
	fd_set master, readfds;
	FD_ZERO(&master);
	FD_SET(ptmx, &master);
	int fdmax=ptmx;
	struct timeval tv;
	
	SDL_FillRect(screen, &(SDL_Rect){0, 0, screen->w, screen->h}, SDL_MapRGB(screen->format, 0, 0, 0));
	SDL_Flip(screen);
	SDL_WM_SetCaption(t.statusline, t.statusline);
	SDL_EnableUNICODE(1);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_Event event;
	bool do_update=true;
	int since_update=0;
	int errupt=0;
	while(!errupt)
	{
		readfds=master;
		tv.tv_sec=0;
		tv.tv_usec=do_update?0:25000;
		if(select(fdmax+1, &readfds, NULL, NULL, &tv)==-1)
		{
			if(errno!=EINTR) // nobody cares if select() was interrupted by a signal
			{
				perror("select");
				errupt++;
			}
		}
		else
		{
			if((FD_ISSET(ptmx, &readfds))&&(since_update++<SINCE_LIMIT))
			{
				char c;
				ssize_t b=read(ptmx, &c, 1);
				if(b==0)
				{
					fprintf(stderr, "termk: EOF, unexpectedly (program exit)\n");
					kill(pid, SIGKILL);
					return(EXIT_SUCCESS);
				}
				else if(b<0) // this is what happens when the child closes
				{
					if(errno!=EIO)
						perror("read");
					kill(pid, SIGKILL);
					return(EXIT_SUCCESS);
				}
				else
				{
					unsigned long uc=0;
					if(t.esc)
					{
						if((c==0x18)||(c==0x1a)) // CAN, SUB cancel escapes (and, not entirely correctly, UTF8)
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
							if(t.escd[0]==0x1b)
							{
								if(t.esc==2)
								{
									switch(c)
									{
										case 'A': // cursor up
											if(t.cur.y>(t.nlines-t.rows)) t.cur.y--;
											t.esc=0;
										break;
										case 'B': // cursor down
											cdown(&t);
											t.esc=0;
										break;
										case 'C': // cursor right
											cright(&t, false);
											t.esc=0;
										break;
										case 'D': // cursor left
											if(t.cur.x) t.cur.x--;
											t.esc=0;
										break;
										case 'F': // set Graphics Mode
											t.meta=true;
											t.esc=0;
										break;
										case 'G': // reset Graphics Mode
											t.meta=false;
											t.esc=0;
										break;
										case 'H': // cursor to home position
											t.cur.x=0;
											t.cur.y=t.nlines-t.rows;
											t.esc=0;
										break;
										case 'I': // reverse line feed
											for(unsigned int i=t.nlines-1;i>0;i--)
											{
												memcpy(t.text[i], t.text[i-1], t.cols);
												memcpy(t.at[i], t.at[i-1], t.cols*sizeof(attr));
												memcpy(t.dev[i], t.dev[i-1], t.cols);
												t.dirty[i][0]=t.dirty[i-1][0];
												t.dirty[i][1]=true;
											}
											t.dirty[0][0]=false;
											t.dirty[0][1]=true;
											memset(t.text[0], ' ', t.cols);
											for(unsigned int i=0;i<=t.cols;i++)
												t.at[0][i]=t.sgr;
											memset(t.dev[0], 0, t.cols+1);
											t.esc=0;
										break;
										case 'J': // clear to end of screen
										{
											unsigned int y=t.cur.y,x=t.cur.x;
											while(y<t.nlines)
											{
												while(x<t.cols)
												{
													t.text[y][x]=' ';
													t.at[y][x]=t.sgr;
													x++;
												}
												x=0;
												t.dirty[y++][0]=true;
											}
										}
											t.esc=0;
										break;
										case 'K': // clear to end of line
										{
											unsigned int x=t.cur.x;
											while(x<t.cols)
											{
												t.text[t.cur.y][x]=' ';
												t.at[t.cur.y][x]=t.sgr;
												x++;
											}
											t.dirty[t.cur.y][0]=true;
										}
											t.esc=0;
										break;
										case 'Y': // cursor move (takes 2 more bytes)
											// nothing
										break;
										case 'Z': // Identify
											do_write(ptmx, "\033/Z"); // "I'm a VT52"
											t.esc=0;
										break;
										case '(': // termk extensions
											// fetch the rest
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
												t.cur.y=min(max(t.escd[2]-32, 0), t.rows-1)+t.nlines-t.rows;
												t.cur.x=min(max(t.escd[3]-32, 0), t.cols-1);
												t.esc=0;
											}
										break;
										case '[': // broken non-vt52 sequence ^[[m sent by some errant programs
											if(c=='m')
												t.esc=0;
										break;
										case '(': // termk extensions
											if(c==')')
											{
												t.escd[t.esc-1]=0;
												const char *escape=t.escd+2;
												if(strcmp(escape, "clr_bol")==0)
												{
													if(clr_bol)
													{
														unsigned int x=t.cur.x;
														while(x)
														{
															x--;
															t.text[t.cur.y][x]=' ';
															t.at[t.cur.y][x]=t.sgr;
														}
														t.dirty[t.cur.y][0]=true;
													}
												}
												else if(strcmp(escape, "save_cursor")==0)
												{
													if(save_cursor)
														t.save=t.cur;
												}
												else if(strcmp(escape, "restore_cursor")==0)
												{
													if(restore_cursor)
													{
														t.dirty[t.cur.y][1]=true;
														t.dirty[(t.cur=t.save).y][1]=true;
													}
												}
												else if(strcmp(escape, "to_status_line")==0)
												{
													if(has_status_line&&to_status_line&&!t.status)
													{
														t.status=true;
														t.statusp=0;
													}
												}
												else if(strcmp(escape, "from_status_line")==0)
												{
													if(has_status_line&&from_status_line&&t.status)
													{
														t.status=false;
														t.statusline[t.statusp]=0;
														SDL_WM_SetCaption(t.statusline, t.statusline);
													}
												}
												else if(strcmp(escape, "dis_status_line")==0)
												{
													if(has_status_line&&dis_status_line)
													{
														t.statusp=0;
														t.statusline[t.statusp]=0;
														SDL_WM_SetCaption("termk", "termk");
													}
												}
												else if(strcmp(escape, "exit_attribute_mode")==0)
												{
													if(exit_attribute_mode)
														t.sgr=DEFAULT_ATTR;
												}
												else if(strcmp(escape, "enter_bold_mode")==0)
												{
													if(enter_bold_mode)
														t.sgr.bold=true;
												}
												else if(strcmp(escape, "enter_reverse_mode")==0)
												{
													if(enter_reverse_mode)
														t.sgr.revvid=true;
												}
												else if(strncmp(escape, "set_foreground:", 15)==0)
												{
													if(set_foreground)
														if(!sscanf(escape+15, "%hhu", &t.sgr.fore))
															fprintf(stderr, "termk: bad set_foreground:%s\n", escape+15);
												}
												else if(strncmp(escape, "set_background:", 15)==0)
												{
													if(set_background)
														if(!sscanf(escape+15, "%hhu", &t.sgr.back))
															fprintf(stderr, "termk: bad set_background:%s\n", escape+15);
												}
												else
												{
													fprintf(stderr, "termk: unknown (escape): %s\n", escape);
												}
												t.esc=0;
											}
										break;
									}
								}
							}
							else // it's UTF8
							{
								if(t.esc>1)
								{
									if((c&0xC0)!=0x80)
									{
										t.esc=0;
										c=0x7f; // replacement character
										goto do_print;
									}
									if((t.escd[0]&0xE0)==0xC0)
									{
										if(t.esc==2)
										{
											t.esc=0;
											uc=((t.escd[0]&0x1F)<<6)|(t.escd[1]&0x3F);
											goto do_print;
										}
									}
									else if((t.escd[0]&0xF0)==0xE0)
									{
										if(t.esc==3)
										{
											t.esc=0;
											uc=((t.escd[0]&0x0F)<<12)|((t.escd[1]&0x3F)<<6)|(t.escd[2]&0x3F);
											goto do_print;
										}
									}
									else if((t.escd[0]&0xF8)==0xF0)
									{
										if(t.esc==4)
										{
											t.esc=0;
											uc=((t.escd[0]&0x07)<<18)|((t.escd[1]&0x3F)<<12)|((t.escd[2]&0x3F)<<6)|(t.escd[3]&0x3F);
											goto do_print;
										}
									}
									else
									{
										c=0x7f;
										goto do_print;
									}
								}
							}
						}
					}
					else if((signed char)c<0)
					{
						if((c&0xC0)==0x80)
						{
							c=0x7f;
							goto do_print;
						}
						else if(((c&0xE0)==0xC0)||((c&0xF0)==0xE0)||((c&0xF8)==0xF0))
						{
							t.esc=1;
							t.escd[0]=c;
						}
						else
						{
							c=0x7f;
							goto do_print;
						}
					}
					else if(c<0x20)
					{
						switch(c)
						{
							case 7: // BEL
								if(dpy)
									XBell(dpy, 100);
							break;
							case 8: // BS
								if(t.cur.x) t.cur.x--;
							break;
							case 9: // HT
								cright(&t, false);
								while((t.cur.x&7) && cright(&t, false));
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
						if(t.status)
						{
							if(t.statusp<81)
							{
								t.statusline[t.statusp++]=uc?0x7f:c;
								t.statusline[t.statusp]=0;
							}
						}
						else if(uc)
						{
							t.text[t.cur.y][t.cur.x]=0x7f;
							t.at[t.cur.y][t.cur.x]=t.sgr;
							t.dirty[t.cur.y][0]=true;
							cright(&t, terminfo?auto_right_margin:false);
						}
						else
						{
							t.text[t.cur.y][t.cur.x]=t.meta?c|0x80:c;
							t.at[t.cur.y][t.cur.x]=t.sgr;
							t.dirty[t.cur.y][0]=true;
							cright(&t, terminfo?auto_right_margin:false);
						}
					}
				}
				do_update=true;
				t.scroll=0;
			}
			else
			{
				since_update=0;
				for(unsigned int i=0;i<t.rows;i++)
				{
					unsigned int j=t.nlines+i-t.rows-t.scroll;
					if(t.dirty[j][0])
					{
						kern(t.text[j], t.dev[j], k);
						#if 0 // for debugging
						for(unsigned int i=0;i<t.cols;i++)
						{
							fprintf(stderr, "%c", "- +"[t.dev[j][i]+1]);
						}
						fprintf(stderr, "\n");
						#endif
						t.dirty[j][0]=false;
						t.dirty[j][1]=true;
					}
					if(t.dirty[j][1]||(j==t.cur.y)||(j==t.old.y)||(t.scroll!=t.scrold))
					{
						SDL_FillRect(screen, &(SDL_Rect){0, 2+i*fsiz.y, screen->w, fsiz.y}, SDL_MapRGB(screen->format, 0, 0, 0));
						dpstr(screen, 4, 2+i*fsiz.y, t.text[j], t.at[j], t.dev[j], j==t.cur.y, t.cur.x, green);
						if(green) filter(screen, (SDL_Rect){0, 2+i*fsiz.y, screen->w, fsiz.y}, (colour){0, 0xc0, 0}, (colour){0, 0x20, 0});
						t.dirty[j][1]=false;
					}
				}
				SDL_Flip(screen);
				t.old=t.cur;
				t.scrold=t.scroll;
				do_update=false;
			}
		}
		while(SDL_PollEvent(&event))
		{
			do_update=true;
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
						SDLMod mod=SDL_GetModState();
						if(key.sym==SDLK_UP)
							do_write(ptmx, (mod&KMOD_CTRL)?"\033[1;5A":"\033A"); // the ctrl-cursors are actually xtermish, not vt52
						else if(key.sym==SDLK_DOWN)
							do_write(ptmx, (mod&KMOD_CTRL)?"\033[1;5B":"\033B");
						else if(key.sym==SDLK_RIGHT)
							do_write(ptmx, (mod&KMOD_CTRL)?"\033[1;5C":"\033C");
						else if(key.sym==SDLK_LEFT)
							do_write(ptmx, (mod&KMOD_CTRL)?"\033[1;5D":"\033D");
						else if(key.sym==SDLK_PAGEUP)
							do_write(ptmx, (mod&KMOD_CTRL)?"\033[5;5~":"\033[5~");
						else if(key.sym==SDLK_PAGEDOWN)
							do_write(ptmx, (mod&KMOD_CTRL)?"\033[6;5~":"\033[6~");
						else if(key.sym==SDLK_F1)
							do_write(ptmx, "\033(f1)");
						else if(key.sym==SDLK_F2)
							do_write(ptmx, "\033(f2)");
						else if(key.sym==SDLK_F3)
							do_write(ptmx, "\033(f3)");
						else if(key.sym==SDLK_F4)
							do_write(ptmx, "\033(f4)");
						else if(key.sym==SDLK_F5)
							do_write(ptmx, "\033(f5)");
						else if(key.sym==SDLK_F6)
							do_write(ptmx, "\033(f6)");
						else if(key.sym==SDLK_F7)
							do_write(ptmx, "\033(f7)");
						else if(key.sym==SDLK_F8)
							do_write(ptmx, "\033(f8)");
						else if(key.sym==SDLK_F9)
							do_write(ptmx, "\033(f9)");
						else if(key.sym==SDLK_F10)
							do_write(ptmx, "\033(f10)");
						else if(key.sym==SDLK_F11)
							do_write(ptmx, "\033(f11)");
						else if(key.sym==SDLK_F12)
							do_write(ptmx, "\033(f12)");
						/* begin readline-isms */
						else if((key.sym==SDLK_HOME)||((key.sym==SDLK_KP7)&&!(mod&KMOD_NUM)))
							do_write(ptmx, "\001"); // C-a
						else if((key.sym==SDLK_END)||((key.sym==SDLK_KP1)&&!(mod&KMOD_NUM)))
							do_write(ptmx, "\005"); // C-e
						else if(key.sym==SDLK_DELETE)
							do_write(ptmx, "\004"); // C-d
						/* end readline-isms */
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
					mouse.x=event.motion.x;
					mouse.y=event.motion.y;
				break;*/
				case SDL_MOUSEBUTTONDOWN:
					/*mouse.x=event.button.x;
					mouse.y=event.button.y;*/
					switch(event.button.button)
					{
						case SDL_BUTTON_WHEELUP:
							t.scroll=min(t.scroll+4, t.nlines-t.rows);
						break;
						case SDL_BUTTON_WHEELDOWN:
							t.scroll=max(t.scroll, 4)-4;
						break;
					}
				break;
			}
		}
	}
	kill(pid, SIGKILL);
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
	t->scroll=0;
	t->sgr=DEFAULT_ATTR;
	t->cur.x=0;
	t->cur.y=nlines-rows;
	t->meta=false;
	t->status=false;
	t->statusp=0;
	strcpy(t->statusline, "termk");
	t->esc=0;
	t->text=malloc(nlines*sizeof(char *));
	if(!t->text)
	{
		perror("initterm: malloc");
		return(-1);
	}
	t->at=malloc(nlines*sizeof(attr *));
	if(!t->at)
	{
		free(t->text);
		perror("initterm: malloc");
		return(-1);
	}
	t->dev=malloc(nlines*sizeof(signed char *));
	if(!t->dev)
	{
		free(t->text);
		free(t->at);
		perror("initterm: malloc");
		return(-1);
	}
	t->dirty=malloc(nlines*sizeof(bool[2]));
	if(!t->dirty)
	{
		free(t->text);
		free(t->at);
		free(t->dev);
		perror("initterm: malloc");
		return(-1);
	}
	for(unsigned int i=0;i<nlines;i++)
	{
		t->dirty[i][0]=false;
		t->dirty[i][1]=true;
		t->text[i]=malloc(cols+1);
		if(!t->text[i])
		{
			perror("initterm: malloc");
			free(t->text[--i]);
			for(;i>0;)
			{
				free(t->text[i]);
				free(t->at[i]);
				free(t->dev[--i]);
			}
			free(t->text);
			free(t->at);
			free(t->dev);
			free(t->dirty);
			return(2);
		}
		t->at[i]=malloc((cols+1)*sizeof(attr));
		if(!t->at[i])
		{
			perror("initterm: malloc");
			free(t->text[i]);
			free(t->at[--i]);
			for(;i>0;)
			{
				free(t->text[i]);
				free(t->at[i]);
				free(t->dev[--i]);
			}
			free(t->text);
			free(t->dev);
			free(t->dirty);
			return(2);
		}
		t->dev[i]=malloc(cols+1);
		if(!t->dev[i])
		{
			perror("initterm: malloc");
			for(;i>0;)
			{
				free(t->text[i]);
				free(t->at[i]);
				free(t->dev[--i]);
			}
			free(t->text);
			free(t->dev);
			free(t->dirty);
			return(2);
		}
		for(unsigned int j=0;j<cols;j++)
		{
			t->text[i][j]=' ';
			t->at[i][j]=DEFAULT_ATTR;
			t->dev[i][j]=0;
		}
		t->text[i][cols]=0;
		t->at[i][cols]=DEFAULT_ATTR;
		t->dev[i][cols]=0;
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
			memcpy(t->at[i], t->at[i+1], t->cols*sizeof(attr));
			memcpy(t->dev[i], t->dev[i+1], t->cols);
			t->dirty[i][0]=t->dirty[i+1][0];
			t->dirty[i][1]=true;
		}
		t->cur.y=t->nlines-1;
		t->dirty[t->cur.y][0]=false;
		t->dirty[t->cur.y][1]=true;
		memset(t->text[t->cur.y], ' ', t->cols);
		for(unsigned int i=0;i<=t->cols;i++)
			t->at[t->cur.y][i]=t->sgr;
		memset(t->dev[t->cur.y], 0, t->cols+1);
	}
}

bool cright(terminal *t, bool wrap)
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
		return(!wrap);
	}
	return(false);
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

void pchar(SDL_Surface *scrn, unsigned int x, unsigned int y, char c, attr a)
{
	if((signed char)c>=32)
	{
		if(a.bold&&bold[(unsigned char)c-32])
			SDL_BlitSurface(bold[(unsigned char)c-32], NULL, scrn, &(SDL_Rect){x, y, 0, 0});
		else
			SDL_BlitSurface(letters[(unsigned char)c-32], NULL, scrn, &(SDL_Rect){x, y, 0, 0});
	}
	else if((signed char)c<0)
	{
		unsigned char d=c-223;
		if(metas[d]) SDL_BlitSurface(metas[d], NULL, scrn, &(SDL_Rect){x, y, 0, 0});
		else SDL_BlitSurface(letters[95], NULL, scrn, &(SDL_Rect){x, y, 0, 0});
	}
	else SDL_BlitSurface(letters[95], NULL, scrn, &(SDL_Rect){x, y, 0, 0});
}

void dpstr(SDL_Surface *scrn, unsigned int x, unsigned int y, const char *s, const attr *at, const signed char *dev, bool iscy, unsigned int cx, bool forcebw)
{
	if(!s) return;
	unsigned int scx=0,ax=x;
	while(s[scx])
	{
		unsigned int i;
		for(i=0;i<nligs;i++)
		{
			if(s[scx]==ligs[i].what[1])
			{
				if((ligs[i].what[0]=='*')||(scx&&(s[scx-1]==ligs[i].what[0])))
				{
					if((ligs[i].what[2]=='*')||(s[scx+1]&&(s[scx+1]==ligs[i].what[2])))
					{
						if((ligs[i].spa[0]=='*')||(scx&&(dev[scx]-dev[scx-1]==ligs[i].spa[0])))
						{
							if((ligs[i].spa[1]=='*')||(s[scx+1]&&(dev[scx+1]-dev[scx]==ligs[i].spa[1])))
								break;
						}
					}
				}
			}
		}
		if(i==nligs)
			pchar(scrn, x+dev[scx], y, s[scx], at[scx]);
		else
		{
			SDL_Surface *l=NULL;
			if(at[scx].bold) l=ligs[i].bold;
			if(!l) l=ligs[i].data;
			SDL_BlitSurface(l, NULL, scrn, &(SDL_Rect){x+dev[scx]+2-(l->w>>1), y, 0, 0});
		}
		x+=fsiz.x;
		scx++;
	}
	scx=0;x=ax;
	while(s[scx])
	{
		SDL_Rect bbox={x+dev[scx], y, fsiz.x+dev[scx+1]-dev[scx], fsiz.y};
		if((iscy&&(scx==cx)))
			invert(scrn, (SDL_Rect){bbox.x, bbox.y, bbox.w, bbox.h-1});
		else if(at[scx].revvid)
			invert(scrn, bbox);
		if(!forcebw)
		{
			colour fore={255, 255, 255}, back={0, 0, 0};
			colourmap(at[scx], fore, back);
			filter(scrn, bbox, fore, back);
		}
		x+=fsiz.x;
		scx++;
	}
}

void colourmap(attr a, colour fore, colour back)
{
	fore[0]=(a.fore&1)?255:0;
	fore[1]=(a.fore&2)?255:0;
	fore[2]=(a.fore&4)?255:0;
	if(a.fore==4) fore[0]=fore[1]=31;
	if(a.bold&&!a.fore)
		fore[0]=fore[1]=fore[2]=85;
	back[0]=(a.back&1)?205:0;
	back[1]=(a.back&2)?205:0;
	back[2]=(a.back&4)?205:0;
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

void filter(SDL_Surface *scrn, SDL_Rect r, colour fore, colour back)
{
	bool wob=true;
	for(unsigned int i=0;i<3;i++)
	{
		if(fore[i]!=255) wob=false;
		if(back[i]) wob=false;
	}
	if(wob) return; // do nothing
	for(int x=r.x;x<r.x+r.w;x++)
		for(int y=r.y;y<r.y+r.h;y++)
		{
			long int s_off = (y*scrn->pitch) + x*scrn->format->BytesPerPixel;
			unsigned char *pixloc = ((unsigned char *)scrn->pixels)+s_off;
			bool set=pixloc[0];
			if(set)
				*(Uint32 *)pixloc=SDL_MapRGB(scrn->format, fore[0], fore[1], fore[2]);
			else
				*(Uint32 *)pixloc=SDL_MapRGB(scrn->format, back[0], back[1], back[2]);
		}	
}

void do_write(int fd, const char *s)
{
	ssize_t l=strlen(s);
	ssize_t b=write(fd, s, l);
	if(b<l)
		perror("write");
}
