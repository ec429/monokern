#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

#include "kfa.h"
#include "bits.h"

int main(int argc, char *argv[])
{
	if(argc!=2)
	{
		fprintf(stderr, "Usage: fontify <outfile.termkf>\n");
		return(1);
	}
	const char *outf=argv[1];
	const char *dot=strrchr(outf, '.');
	if(!dot)
	{
		fprintf(stderr, "Usage: fontify <outfile.termkf>\n");
		return(1);
	}
	size_t len=dot-outf;
	char *name=malloc(len);
	if(!name)
	{
		perror("fontify: malloc");
		return(1);
	}
	strncpy(name, outf, len);
	DIR *curdir=opendir(name);
	if(!curdir)
	{
		fprintf(stderr, "fontify: opendir(\"%s\"): %s\n", name, strerror(errno));
		return(1);
	}
	unsigned int nkf=0;
	kf_entry *kfe=NULL;
	struct dirent *entry;
	while((entry=readdir(curdir)))
	{
		const char *fname=entry->d_name;
		off_t flen=strlen(fname);
		if(strcmp(fname+flen-4, ".pbm")) continue;
		if(flen>64)
		{
			fprintf(stderr, "fontify: filename too long: %s\n", fname);
			return(1);
		}
		char *fullname=malloc(len+flen+2);
		if(!fullname)
		{
			perror("fontify: malloc");
			return(1);
		}
		snprintf(fullname, len+flen+2, "%s/%s", name, fname);
		FILE *fp=fopen(fullname, "r");
		if(!fp)
		{
			fprintf(stderr, "fontify: fopen(\"%s\"): %s\n", fullname, strerror(errno));
			return(1);
		}
		string data=sslurp(fp);
		kf_entry e={.name=make_string(fname), .data=data};
		fclose(fp);
		unsigned int n=nkf++;
		kf_entry *nkfe=realloc(kfe, nkf*sizeof(kf_entry));
		if(!nkfe)
		{
			nkf=n;
			perror("fontify: realloc");
			return(1);
		}
		(kfe=nkfe)[n]=e;
	}
	closedir(curdir);
	FILE *fout=fopen(outf, "w");
	if(!fout)
	{
		fprintf(stderr, "fontify: fopen(\"%s\"): %s\n", outf, strerror(errno));
		return(1);
	}
	fputshort(nkf, fout);
	unsigned int koff=2+nkf*73;
	for(unsigned int i=0;i<nkf;i++)
	{
		fputc(kfe[i].name.i, fout);
		for(unsigned int b=0;b<64;b++)
			if(b<kfe[i].name.i) fputc(kfe[i].name.buf[b], fout); else fputc(0, fout);
		fputlong(koff, fout);
		fputlong(kfe[i].data.i, fout);
		koff+=kfe[i].data.i;
		free_string(&kfe[i].name);
	}
	for(unsigned int i=0;i<nkf;i++)
	{
		for(unsigned int b=0;b<kfe[i].data.i;b++)
			fputc(kfe[i].data.buf[b], fout);
		free_string(&kfe[i].data);
	}
	fclose(fout);
	free(name);
	return(0);
}
