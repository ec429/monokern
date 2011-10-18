#include "kern.h"

struct _kern
{
	int score[96][96][3];
};

int rate(const char *str, const signed char *dev, const KERN k);
int ekern(const char *str, signed char *dev, const KERN k);

KERN kern_init(FILE *fp)
{
	KERN rv;
	for(int i=0;i<96;i++)
		for(int j=0;j<96;j++)
			rv.score[i][j][0]=rv.score[i][j][1]=rv.score[i][j][2]=0;
	if(!fp) return(rv);
	while(!feof(fp))
	{
		int a=fgetc(fp);
		if(a==EOF) break;
		int b=fgetc(fp);
		if(b==EOF) break;
		a-=32;
		b-=32;
		if((a<0)||(a>95)) break;
		if((b<0)||(b>95)) break;
		if(fgetc(fp)!=' ') break;
		fscanf(fp, "%d %d %d\n", rv.score[a][b], rv.score[a][b]+1, rv.score[a][b]+2);
	}
	return(rv);
}

int kern(const char *str, signed char *dev, const KERN k);
