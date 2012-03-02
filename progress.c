#include <stdio.h>

//#include "bits.h"
#include "kern.h"
#include "kern_hack.h"

int main(int argc, char *argv[])
{
	const char *font="as";
	for(int arg=1;arg<argc;arg++)
	{
		if(argv[arg][0]=='@')
			font=argv[arg]+1;
		else
			fprintf(stderr, "Bad arg: %s\n", argv[arg]);
	}
	char kfn[strlen(font)+8];
	snprintf(kfn, strlen(font)+8, "%s/scores", font);
	FILE *kf=fopen(kfn, "r");
	KERN *k=kern_init(kf, false);
	fclose(kf);
	if(!k)
	{
		fprintf(stderr, "progress: kern_init failed\n");
		return(EXIT_FAILURE);
	}
	unsigned char ok=0, part=0, weak=0, none=0;
	for(unsigned char i=0;i<96;i++)
	{
		unsigned char scored=0;
		for(unsigned char j=0;j<96;j++)
			if(k->score[i][j][0]||k->score[i][j][1]) scored++;
		if(scored>64) {printf("%c: ok\t", i+32); ok++;}
		else if(scored>32) {printf("%c: part\t", i+32); part++;}
		else if(scored) {printf("%c: WEAK\t", i+32); weak++;}
		else {printf("%c: NONE\t", i+32); none++;}
		printf("(%d)\n", scored);
	}
	fprintf(stderr, "ok %hhu, part %hhu, weak %hhu, none %hhu\n", ok, part, weak, none);
	return(EXIT_SUCCESS);
}
