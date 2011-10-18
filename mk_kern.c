#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include <SDL_image.h>
#include <time.h>

SDL_Surface *ginit(unsigned int w, unsigned int h, unsigned char bpp);
void pchar(SDL_Surface *scrn, unsigned int x, unsigned int y, char c);
void pstr(SDL_Surface *scrn, unsigned int x, unsigned int y, const char *s);

SDL_Surface *letters[26];

int main(int argc, char **argv)
{
	char start[2]={0,0};
	for(int arg=1;arg<argc;arg++)
	{
		if(strncmp(argv[arg], "--start=", 8)==0)
		{
			start[0]=argv[arg][8]-97;
			start[1]=argv[arg][9]-97;
			if((start[0]<0)||(start[0]>25))
			{
				fprintf(stderr, "bad start[0] %c\n", start[0]+97);
				return(1);
			}
			if((start[1]<0)||(start[1]>25))
			{
				fprintf(stderr, "bad start[1] %c\n", start[1]+97);
				return(1);
			}
		}
	}
	srand(time(NULL));
	SDL_Surface *screen=ginit(320, 240, 32);
	if(!screen)
	{
		fprintf(stderr, "ginit: %s\n", SDL_GetError());
		return(1);
	}
	for(int i=0;i<26;i++)
	{
		char lfn[13];
		sprintf(lfn, "az/az_%hhu.pbm", i);
		if(!(letters[i]=IMG_Load(lfn)))
		{
			fprintf(stderr, "IMG_Load: %s\n", IMG_GetError());
			return(1);
		}
	}
	for(int i=start[0];i<26;i++)
	{
		for(int j=(i==start[0]?start[1]:0);j<26;j++)
		{
			SDL_FillRect(screen, &(SDL_Rect){0, 0, 320, 240}, SDL_MapRGB(screen->format, 0, 0, 0));
			pchar(screen, 8, 8, i+97);
			pchar(screen, 211, 8, j+97);
			for(int y=0;y<12;y++)
			{
				char word[9];
				for(int k=0;k<8;k++)
					word[k]=97+((rand()*26.0)/RAND_MAX);
				word[8]=0;
				int n=1+(rand()*5.0)/RAND_MAX;
				word[n]=i+97;
				word[n+1]=0;
				pstr(screen, 8, 24+(y*14), word);
				pstr(screen, 88, 24+(y*14), word);
				pstr(screen, 168, 24+(y*14), word);
				word[n+1]=j+97;
				pstr(screen, 13+6*n, 24+(y*14), word+n+1);
				pstr(screen, 94+6*n, 24+(y*14), word+n+1);
				pstr(screen, 175+6*n, 24+(y*14), word+n+1);
			}
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
								else if(key.sym==SDLK_1)
								{
									printf("%c%c n\n", i+97, j+97);
									errupt++;
								}
								else if(key.sym==SDLK_2)
								{
									printf("%c%c m\n", i+97, j+97);
									errupt++;
								}
								else if(key.sym==SDLK_4)
								{
									printf("%c%c w\n", i+97, j+97);
									errupt++;
								}
								else if(key.sym==SDLK_3)
								{
									printf("%c%c nm\n", i+97, j+97);
									errupt++;
								}
								else if(key.sym==SDLK_5)
								{
									printf("%c%c nw\n", i+97, j+97);
									errupt++;
								}
								else if(key.sym==SDLK_6)
								{
									printf("%c%c mw\n", i+97, j+97);
									errupt++;
								}
								else if(key.sym==SDLK_7)
								{
									printf("%c%c nmw\n", i+97, j+97);
									errupt++;
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
	SDL_BlitSurface(letters[(unsigned char)c-97], NULL, scrn, &(SDL_Rect){x, y, 5, 8});
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
