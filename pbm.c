#include "pbm.h"
#include <stdbool.h>

SDL_Surface *pbm_string(string s)
{
	if(!s.buf) return(NULL);
	if(s.i<3) return(NULL);
	if(strncmp(s.buf, "P4\n", 3)) return(NULL);
	unsigned int w, h;
	unsigned int i=3,j=i;
	while(i<s.i)
	{
		if(s.buf[i]==' ') break;
		i++;
	}
	s.buf[i++]=0;
	sscanf(s.buf+j, "%u", &w);
	j=i;
	if(w>8) return(NULL);
	while(i<s.i)
	{
		if(s.buf[i]=='\n') break;
		i++;
	}
	s.buf[i++]=0;
	sscanf(s.buf+j, "%u", &h);
	SDL_Surface *rv=SDL_CreateRGBSurface(SDL_HWSURFACE, w, h, 24, 0xFF0000, 0xFF00, 0xFF, 0);
	if(!rv) return(NULL);
	for(unsigned int y=0;y<h;y++)
	{
		if(i<s.i)
		{
			for(unsigned int x=0;x<w;x++)
			{
				bool px=s.buf[i]&(1<<(7-x));;
				SDL_FillRect(rv, &(SDL_Rect){x, y, 1, 1}, SDL_MapRGB(rv->format, px?0:255, px?0:255, px?0:255));
			}
		}
		else
		{
			SDL_FreeSurface(rv);
			return(NULL);
		}
		i++;
	}
	return(rv);
}