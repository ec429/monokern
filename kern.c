#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include "kern.h"

struct _kern
{
	int score[96][96][3];
};

int rate(size_t n, const char *str, const signed char *dev, const KERN *k);
int ekern(size_t n, const char *str, signed char *dev, const KERN *k);

KERN *kern_init(FILE *fp)
{
	string s=sslurp(fp);
	return(kern_init_s(s));
}

KERN *kern_init_s(string s)
{
	KERN *rv=malloc(sizeof(KERN));
	for(unsigned int i=0;i<96;i++)
		for(unsigned int j=0;j<96;j++)
			rv->score[i][j][0]=(rv->score[i][j][1]=rv->score[i][j][2]=0)-15;
	if(!s.buf) return(rv);
	unsigned int i=0;
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
		if((a<0)||(a>94)) break;
		if((b<0)||(b>94)) break;
		if(p[2]!=' ') break;
		if(sscanf(p+3, "%d %d %d", rv->score[a][b], rv->score[a][b]+1, rv->score[a][b]+2)!=3)
		{
			free(rv);
			return(NULL);
		}
		p[l]=c;
		i+=l+1;
	}
	return(rv);
}

int rate(size_t n, const char *str, const signed char *dev, const KERN *k)
{
	int rv=0;
	size_t i=0;
	while(i<n-1)
	{
		int spa=dev[i+1]-dev[i];
		int a=str[i],b=str[++i];
		int w=4;
		if(isalpha(a)) w+=2;
		if(isalpha(b)) w+=2;
		if(a=='_') w=(w|1)/2;
		if(b=='_') w=(w|1)/2;
		a-=32;
		b-=32;
		if((a<0)||(a>94)||(b<0)||(b>94))
		{
			rv-=spa?30:0;
			continue;
		}
		if(!(a&&b))
		{
			continue;
		}
		if(spa<-1)
		{
			rv+=(k->score[a][b][0]-30);
			continue;
		}
		if(spa>1)
		{
			rv+=(k->score[a][b][2]-30)*w;
			continue;
		}
		if(i)
		{
			int c=str[i-2];
			c-=32;
			if((c>=0)&&(c<96))
			{
				int osp=dev[i-1]-dev[i-2];
				if((k->score[a][b][0]>0)&&(k->score[c][a][0]>0))
					if(abs(osp-spa)>1) rv-=30*w;
			}
		}
		rv+=k->score[a][b][spa+1]*w;
	}
	return(rv);
}

int ekern(size_t n, const char *str, signed char *dev, const KERN *k)
{
	if(n<3)
	{
		return(rate(n, str, dev, k));
	}
	else if(n==3)
	{
		int maxsc=INT_MIN, mx=0;
		for(int i=0;i<3;i++)
		{
			dev[1]=i-1;
			int sc=rate(3, str, dev, k);
			if(sc>maxsc)
			{
				maxsc=sc;
				mx=i;
			}
		}
		dev[1]=mx-1;
		return(maxsc);
	}
	else if(n==4)
	{
		int maxsc=INT_MIN, mi=0, mj=0;
		for(int i=0;i<3;i++)
		{
			dev[1]=i-1;
			for(int j=0;j<3;j++)
			{
				dev[2]=j-1;
				int sc=rate(4, str, dev, k);
				if(sc>maxsc)
				{
					maxsc=sc;
					mi=i;
					mj=j;
				}
			}
		}
		dev[1]=mi-1;
		dev[2]=mj-1;
		return(maxsc);
	}
	else if(n>40)
	{
		for(unsigned int i=0;i<n;i++)
			dev[i]=0;
		return(0);
	}
	else
	{
		int s=n>>1;
		int maxsc=INT_MIN, mx=0;
		for(int i=0;i<3;i++)
		{
			dev[s]=i-1;
			int sc=ekern(s+1, str, dev, k)+ekern(n-s, str+s, dev+s, k);
			if(sc>maxsc)
			{
				maxsc=sc;
				mx=i;
			}
		}
		dev[s]=mx-1;
		return(ekern(s+1, str, dev, k)+ekern(s+1, str+s, dev+s, k));
	}
}

int kern(const char *str, signed char *dev, const KERN *k)
{
	if(!(str&&dev&&k)) return(0);
	int score=0;
	while(*str)
	{
		if((*str==' ')||(*str==0x7f)||(*str<0))
		{
			*dev++=0;
			str++;
			continue;
		}
		size_t p=0;
		while(!((str[p]==' ')||(str[p]==0x7f)||(str[p]<=0))) p++;
		int maxsc=INT_MIN, mi=0, mj=0;
		for(int i=0;i<3;i++)
		{
			dev[0]=i-1;
			for(int j=0;j<3;j++)
			{
				if(!i&&!j) continue;
				if(i+j>2) continue;
				dev[p-1]=j-1;
				int sc=ekern(p, str, dev, k);
				if(sc>maxsc)
				{
					maxsc=sc;
					mi=i;
					mj=j;
				}
			}
		}
		dev[0]=mi-1;
		dev[p-1]=mj-1;
		score+=ekern(p, str, dev, k);
		str+=p;
		dev+=p;
	}
	return(score);
}
