#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include <SDL_image.h>

#include "kern.h"

SDL_Surface *ginit(unsigned int w, unsigned int h, unsigned char bpp);
void pchar(SDL_Surface *scrn, unsigned int x, unsigned int y, char c);
void pstr(SDL_Surface *scrn, unsigned int x, unsigned int y, const char *s);
void dpstr(SDL_Surface *scrn, unsigned int x, unsigned int y, const char *s, const signed char *dev);
void kdstr(SDL_Surface *scrn, unsigned int x, unsigned int y, const char *s, const KERN *k);

void init_char(char **buf, int *l, int *i);
void append_char(char **buf, int *l, int *i, char c);
char * fgetl(FILE *fp);

SDL_Surface *letters[95];

int main(void)
{
	SDL_Surface *screen=ginit(500, 320, 32);
	if(!screen)
	{
		fprintf(stderr, "ginit: %s\n", SDL_GetError());
		return(1);
	}
	for(int i=0;i<94;i++)
	{
		char lfn[14];
		sprintf(lfn, "as/as_%hhu.pbm", i+32);
		if(!(letters[i]=IMG_Load(lfn)))
		{
			fprintf(stderr, "IMG_Load: %s\n", IMG_GetError());
			return(1);
		}
	}
	FILE *kf=fopen("scores", "r");
	KERN *k=kern_init(kf);
	fclose(kf);
	int y=3;
	char *p;
	while((p=fgetl(stdin)))
	{
		char c=*p;
		if(c) kdstr(screen, 4, y, p, k);
		free(p);
		y+=13;
		if(feof(stdin)) break;
		if(y>308) break;
	}
	/*kdstr(screen, 4, 3, "This is some sample text to be kerned.  Hopefully it should look rather nice (though it probably won't!)", k);
	kdstr(screen, 4, 16, "Here's another line of text.  It will also be kerned, by the same algorithm; pretty cool huh? :-)", k);*/
	SDL_Flip(screen);
	SDL_Event event;
	int errupt=0;
	while(!errupt)
	{
		if(SDL_PollEvent(&event))
		{
			switch(event.type)
			{
				case SDL_QUIT:
					return(1);
				break;
				case SDL_KEYDOWN:
					if(event.key.type==SDL_KEYDOWN)
					{
						SDL_keysym key=event.key.keysym;
						if(key.sym==SDLK_q)
						{
							return(1);
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
		else
			SDL_Delay(20);
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
	if((c>=32)&&(c<127))
		SDL_BlitSurface(letters[(unsigned char)c-32], NULL, scrn, &(SDL_Rect){x, y, 5, 8});
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

void dpstr(SDL_Surface *scrn, unsigned int x, unsigned int y, const char *s, const signed char *dev)
{
	if(!s) return;
	while(*s)
	{
		pchar(scrn, x+*dev++, y, *s++);
		x+=6;
	}
}

void kdstr(SDL_Surface *scrn, unsigned int x, unsigned int y, const char *s, const KERN *k)
{
	signed char *dev=malloc(strlen(s));
	kern(s, dev, k);
	dpstr(scrn, x, y, s, dev);
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
