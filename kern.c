#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include "kern.h"
#include "kern_hack.h"

int ratepair(const char pair[2], const signed char dev[2], const KERN *k);

KERN *kern_init(FILE *fp, bool guess)
{
	string s=sslurp(fp);
	KERN *k=kern_init_s(s, guess);
	free_string(&s);
	return(k);
}

KERN *kern_init_s(string s, bool guess)
{
	KERN *rv=malloc(sizeof(KERN));
	for(unsigned int i=0;i<96;i++)
		for(unsigned int j=0;j<96;j++)
			rv->score[i][j][0]=(rv->score[i][j][1]=0)-30;
	if(!s.buf) return(rv);
	unsigned int i=0;
	bool notblank[96];
	memset(notblank, 0, 96*sizeof(bool));
	while(i<s.i)
	{
		char *p=s.buf+i;
		unsigned int l=strcspn(p, "\n");
		unsigned char c=p[l];
		p[l]=0;
		int a=p[0];
		int b=p[1];
		a-=32;
		b-=32;
		if((a<0)||(a>95)) break;
		if((b<0)||(b>95)) break;
		if(p[2]!=' ') break;
		if(sscanf(p+3, "%hhd %hhd", rv->score[a][b], rv->score[a][b]+1)!=2)
		{
			free(rv);
			return(NULL);
		}
		if(rv->score[a][b][0]||rv->score[a][b][1]) notblank[a]=true;
		else if(guess&&!notblank[a]) rv->score[a][b][0]=rv->score[a][b][1]=-6;
		p[l]=c;
		i+=l+1;
	}
	if(i<s.i)
	{
		free(rv);
		return(NULL);
	}
	return(rv);
}

int ratepair(const char pair[2], const signed char dev[2], const KERN *k)
{
	int spa=dev[1]-dev[0];
	char a=pair[0], b=pair[1];
	int w=4;
	if(isalpha(a)) w+=2;
	if(isalpha(b)) w+=2;
	if(a=='_') w=(w|1)/2;
	if(b=='_') w=(w|1)/2;
	a-=32;
	b-=32;
	if((a<0)||(a>95)||(b<0)||(b>95))
		return((spa<0)?-30:0);
	if(!(a&&b))
		return(0);
	if(spa<-1)
		return((k->score[(unsigned char)a][(unsigned char)b][0]-30)*w);
	if(spa>1)
		return((k->score[(unsigned char)a][(unsigned char)b][1]-30)*w);
	/*if(i)
	{
		int c=str[i-2];
		c-=32;
		if((c>=0)&&(c<96))
		{
			int osp=dev[i-1]-dev[i-2];
			if((k->score[a][b][0]>0)&&(k->score[c][a][0]>0))
				if(abs(osp-spa)>1) rv-=30*w;
		}
	}*/
	if(!spa) return(0);
	if(spa==1) return(k->score[(unsigned char)a][(unsigned char)b][1]*w);
	return(k->score[(unsigned char)a][(unsigned char)b][0]*w);
}

int kern(const char *str, signed char *dev, const KERN *k)
{
	if(!(str&&dev&&k)) return(-1);
	unsigned int n=strlen(str);
	unsigned char rdev[n+1][3];
	for(unsigned int r=0;r<3;r++)
		rdev[0][r]=r;
	int rscore[3]={0,0,0};
	int nrscore[3];
	unsigned int nrms[3];
	for(unsigned int i=1;i<=n;i++)
	{
		nrscore[0]=nrscore[1]=nrscore[2]=INT_MIN;
		nrms[0]=nrms[1]=nrms[2]=1;
		for(unsigned int l=0;l<3;l++)
		{
			for(unsigned int r=0;r<3;r++)
			{
				int score=ratepair((char[2]){str[i-1], str[i]}, (signed char[2]){l, r}, k);
				/*if((i>1)&&isalpha(str[i-2])) // force balance - note that this breaks the guarantee of optimality
				{
					if((l<r)&&(rdev[i-2][l]>l)) score-=30;
					else if((l>r)&&(rdev[i-2][l]<l)) score-=30;
				}*/
				score+=rscore[l];
				if((score>nrscore[r])||((score==nrscore[r])&&(l==1)))
				{
					nrscore[r]=score;
					nrms[r]=l;
				}
			}
		}
		for(unsigned int r=0;r<3;r++)
			rdev[i][r]=nrms[r];
		for(unsigned int r=0;r<3;r++)
		{
			rscore[r]=nrscore[nrms[r]];
		}
	}
	int mr=1;
	for(unsigned int r=0;r<3;r++)
	{
		if(rscore[r]>rscore[mr])
			mr=r;
	}
	for(unsigned int j=n;j>0;j--)
	{
		mr=rdev[j][mr];
		dev[j-1]=mr-1;
	}
	return(0);
}
