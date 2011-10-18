#include <stdio.h>

int main(void)
{
	int score[95][95][3];
	for(int i=0;i<95;i++)
		for(int j=0;j<95;j++)
			score[i][j][0]=score[i][j][1]=score[i][j][2]=0;
	while(!feof(stdin))
	{
		int a=getchar();
		if(a==EOF) break;
		int b=getchar();
		if(b==EOF) break;
		a-=32;
		b-=32;
		if((a<0)||(a>94)) break;
		if((b<0)||(b>94)) break;
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
	for(int i=0;i<95;i++)
		for(int j=0;j<95;j++)
			printf("%c%c %d %d %d\n", i+32, j+32, score[i][j][0], score[i][j][1], score[i][j][2]);
	return(0);
}
