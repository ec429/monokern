#include <stdio.h>
#include <time.h>
#include <SDL.h>

#include "bits.h"
#include "kern.h"
#include "kern_hack.h"
#include "pbm.h"

typedef struct
{
	SDL_Surface *data;
	unsigned char what[3];
	signed char spa[2];
}
lig;

typedef struct
{
	char *name;
	SDL_Surface *data;
}
lid;

SDL_Surface *letters[96];
unsigned int nlids;
lid *lids;
unsigned int nligs;
lig *ligs;
unsigned int sy=13, sx=6;

SDL_Surface *ginit(unsigned int w, unsigned int h, unsigned char bpp);
void pchar(SDL_Surface *scrn, unsigned int x, unsigned int y, char c);
void pstr(SDL_Surface *scrn, unsigned int x, unsigned int y, const char *s);

int main(int argc, char *argv[])
{
	unsigned char i=0, last=1;
	const char *font="as";
	for(int arg=1;arg<argc;arg++)
	{
		if(strlen(argv[arg])==1)
			i=argv[arg][0]-32;
		else if(argv[arg][0]=='-')
			last=argv[arg][1]-32;
		else if(argv[arg][0]=='@')
			font=argv[arg]+1;
		else
			fprintf(stderr, "Bad arg: %s\n", argv[arg]);
	}
	srand(time(NULL));
	if(!i)
		i=(rand()*95.0/RAND_MAX)+1;
	for(int i=0;i<96;i++)
	{
		char lfn[strlen(font)+12];
		snprintf(lfn, strlen(font)+12, "%s/as_%hhu.pbm", font, i+32);
		FILE *fp=fopen(lfn, "r");
		if(!fp)
		{
			perror("edkern: fopen");
			return(EXIT_FAILURE);
		}
		string data=sslurp(fp);
		if(!(letters[i]=pbm_string(data)))
		{
			fprintf(stderr, "edkern: pbm_string failed: %s\n", lfn);
			return(EXIT_FAILURE);
		}
	}
	char kfn[strlen(font)+8];
	snprintf(kfn, strlen(font)+8, "%s/scores", font);
	FILE *kf=fopen(kfn, "r");
	KERN *k=kern_init(kf, false);
	fclose(kf);
	if(!k)
	{
		fprintf(stderr, "edkern: kern_init failed\n");
		return(EXIT_FAILURE);
	}
	if(letters[0])
	{
		sy=letters[0]->h+1;
		sx=letters[0]->w+1;
	}
	SDL_Surface *screen=ginit(20+50*sx, 32+16*sy, 32);
	if(!screen)
	{
		fprintf(stderr, "edkern: ginit failed: %s\n", SDL_GetError());
		return(EXIT_FAILURE);
	}
	for(unsigned char j=last;j<96;j++)
	{
		repeat:
		SDL_FillRect(screen, &(SDL_Rect){0, 0, screen->w, screen->h}, SDL_MapRGB(screen->format, 0, 0, 0));
		SDL_FillRect(screen, &(SDL_Rect){screen->w-94-sx, 8, 94+sx, sy-1}, SDL_MapRGB(screen->format, 31, 31, 31));
		pchar(screen, screen->w-95-sx+j, 8, '>');
		pchar(screen, 8, 8, i+32);
		pchar(screen, 7+34*sx, 8, j+32);
		for(int y=0;y<12;y++)
		{
			char word[9];
			for(int k=0;k<8;k++)
				word[k]=32+((rand()*95.0)/RAND_MAX);
			word[8]=0;
			int n=1+(rand()*5.0)/RAND_MAX;
			word[n]=i+32;
			word[n+1]=0;
			pstr(screen, 8, 36+(y*sy), word);
			pstr(screen, 8+12*sx, 36+(y*sy), word);
			pstr(screen, 8+24*sx, 36+(y*sy), word);
			word[n+1]=j+32;
			pstr(screen, 7+sx*(n+1), 36+(y*sy), word+n+1);
			pstr(screen, 8+sx*(n+13), 36+(y*sy), word+n+1);
			pstr(screen, 9+sx*(n+25), 36+(y*sy), word+n+1);
		}
		SDL_Flip(screen);
		SDL_Event event;
		int errupt=0;
		while(!errupt)
		{
			{
				char num[16];
				snprintf(num, 16, "%d    ", k->score[i][j][0]);
				pstr(screen, 8+4*sx, 12, num);
				snprintf(num, 16, "%d    ", k->score[i][j][1]);
				pstr(screen, 8+25*sx, 12, num);
				SDL_Flip(screen);
			}
			if(SDL_PollEvent(&event))
			{
				switch(event.type)
				{
					case SDL_QUIT:
						errupt=2;
					break;
					case SDL_KEYDOWN:
						if(event.key.type==SDL_KEYDOWN)
						{
							SDL_keysym key=event.key.keysym;
							if(key.sym==SDLK_q)
								errupt=2;
							else if(key.sym==SDLK_r)
								goto repeat;
							else if(key.sym==SDLK_SPACE)
								errupt=1;
							else if(key.sym==SDLK_RETURN)
								errupt=1;
							else if(key.sym==SDLK_a)
							{
								if(k->score[i][j][0]==-30) k->score[i][j][0]=0;
								k->score[i][j][0]++;
							}
							else if(key.sym==SDLK_s)
								k->score[i][j][0]=0;
							else if(key.sym==SDLK_d)
								k->score[i][j][0]--;
							else if(key.sym==SDLK_x)
								k->score[i][j][0]=-30;
							else if(key.sym==SDLK_j)
								k->score[i][j][1]--;
							else if(key.sym==SDLK_k)
								k->score[i][j][1]=0;
							else if(key.sym==SDLK_l)
								k->score[i][j][1]++;
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
			else
				SDL_Delay(20);
		}
		if(errupt==2) break;
	}
	FILE *of=fopen(kfn, "w");
	for(unsigned int a=0;a<96;a++)
	{
		for(unsigned int b=0;b<96;b++)
		{
			fputc(a+32, of);
			fputc(b+32, of);
			fprintf(of, " %d %d\n", k->score[a][b][0], k->score[a][b][1]);
		}
	}
	return(0);
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
	SDL_BlitSurface(letters[(unsigned char)c-32], NULL, scrn, &(SDL_Rect){x, y, 0, 0});
}

void pstr(SDL_Surface *scrn, unsigned int x, unsigned int y, const char *s)
{
	if(!s) return;
	while(*s)
	{
		pchar(scrn, x, y, *s++);
		x+=sx;
	}
}
