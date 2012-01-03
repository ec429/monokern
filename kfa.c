#include "kfa.h"

int kf_read(FILE *kfa, kf_archive *buf)
{
	if(!kfa) return(-1);
	if(!buf) return(-1);
	buf->nents=fgetshort(kfa);
	buf->ents=malloc(buf->nents*sizeof(kf_entry));
	if(!buf->ents)
	{
		buf->nents=0;
		return(-1);
	}
	for(unsigned int i=0;i<buf->nents;i++)
	{
		fseek(kfa, 2+i*72, SEEK_SET);
		buf->ents[i].name=null_string();
		for(unsigned int b=0;b<64;b++)
			append_char(&buf->ents[i].name, fgetc(kfa));
		unsigned int koff=fgetlong(kfa), klen=fgetlong(kfa);
		fseek(kfa, koff, SEEK_SET);
		buf->ents[i].data=null_string();
		for(unsigned int b=0;b<klen;b++)
			append_char(&buf->ents[i].data, fgetc(kfa));
	}
	return(buf->nents);
}

void kf_free(kf_archive kfb)
{
	if(kfb.ents)
		for(unsigned int i=0;i<kfb.nents;i++)
		{
			free_string(&kfb.ents[i].name);
			free_string(&kfb.ents[i].data);
		}
	free(kfb.ents);
}
