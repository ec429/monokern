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

SDL_Surface *ginit(unsigned int w, unsigned int h, unsigned char bpp);

int main(int argc, char *argv[])
{
	unsigned char first=0;
	for(int arg=1;arg<argc;arg++)
	{
		if(strlen(argv[arg])==1)
			first=argv[arg][0];
		else
			fprintf(stderr, "Bad arg: %s\n", argv[arg]);
	}
	if(!first)
	{
		srand(time(NULL));
		first=(rand()*95/RAND_MAX)+33;
	}
	for(int i=0;i<96;i++)
	{
		char lfn[14];
		sprintf(lfn, "as/as_%hhu.pbm", i+32);
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
	FILE *kf=fopen("as/scores", "r");
	KERN *k=kern_init(kf);
	fclose(kf);
	if(!k)
	{
		fprintf(stderr, "edkern: kern_init failed\n");
		return(EXIT_FAILURE);
	}
	SDL_Surface *screen=ginit(500, 320, 32);
	if(!screen)
	{
		fprintf(stderr, "edkern: ginit failed: %s\n", SDL_GetError());
		return(EXIT_FAILURE);
	}
	FILE *of=fopen("as/scores", "w");
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
