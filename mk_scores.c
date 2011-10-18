#include <stdio.h>

int main(void)
{
	int score[26][26][3];
	for(int i=0;i<26;i++)
		for(int j=0;j<26;j++)
			score[i][j][0]=score[i][j][1]=score[i][j][2]=0;
	while(!feof(stdin))
	{
		int a=getchar();
		if(a==EOF) break;
		int b=getchar();
		if(b==EOF) break;
		a-=97;
		b-=97;
		if((a<0)||(a>25)) break;
		if((b<0)||(b>25)) break;
		if(getchar()!=' ') break;
		int c,n=0,m=0,w=0;
		while((c=getchar())!='\n')
		{
			if(c==EOF) break;
			else if(c=='n') n++;
			else if(c=='m') m++;
			else if(c=='w') w++;
		}
		score[a][b][0]+=n?(n*4):-30;
		score[a][b][1]+=m*3+w;
		score[a][b][2]+=w*4-(m+n);
	}
	for(int i=0;i<26;i++)
		for(int j=0;j<26;j++)
			printf("%c%c %d %d %d\n", i+97, j+97, score[i][j][0], score[i][j][1], score[i][j][2]);
	return(0);
}
