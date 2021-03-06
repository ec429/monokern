#include "bits.h"

char *fgetl(FILE *fp)
{
	string s=init_string();
	signed int c;
	while(!feof(fp))
	{
		c=fgetc(fp);
		if((c==EOF)||(c=='\n'))
			break;
		if(c!=0)
		{
			append_char(&s, c);
		}
	}
	return(s.buf);
}

string sslurp(FILE *fp)
{
	string s=init_string();
	signed int c;
	while(!feof(fp))
	{
		c=fgetc(fp);
		if(c==EOF)
			break;
		append_char(&s, c);
	}
	return(s);
}

void append_char(string *s, char c)
{
	if(s->buf)
	{
		s->buf[s->i++]=c;
	}
	else
	{
		*s=init_string();
		append_char(s, c);
	}
	char *nbuf=s->buf;
	if(s->i>=s->l)
	{
		if(s->i)
			s->l=s->i*2;
		else
			s->l=80;
		nbuf=(char *)realloc(s->buf, s->l);
	}
	if(nbuf)
	{
		s->buf=nbuf;
		s->buf[s->i]=0;
	}
	else
	{
		free(s->buf);
		*s=init_string();
	}
}

void append_str(string *s, const char *str)
{
	while(str && *str) // not the most tremendously efficient implementation, but conceptually simple at least
	{
		append_char(s, *str++);
	}
}

void append_string(string *s, const string t)
{
	if(!s) return;
	size_t i=s->i+t.i;
	if(i>=s->l)
	{
		size_t l=i+1;
		char *nbuf=(char *)realloc(s->buf, l);
		if(!nbuf) return;
		s->l=l;
		s->buf=nbuf;
	}
	memcpy(s->buf+s->i, t.buf, t.i);
	s->buf[i]=0;
	s->i=i;
}

string init_string(void)
{
	string s;
	s.buf=(char *)malloc(s.l=80);
	if(s.buf)
		s.buf[0]=0;
	else
		s.l=0;
	s.i=0;
	return(s);
}

string null_string(void)
{
	return((string){.buf=NULL, .l=0, .i=0});
}

string make_string(const char *str)
{
	string s=init_string();
	append_str(&s, str);
	return(s);
}

string dup_string(const string s)
{
	string rv={.buf=malloc(s.i+1), s.i+1, s.i};
	if(!rv.buf) return(null_string());
	memcpy(rv.buf, s.buf, s.i+1);
	return(rv);
}

void free_string(string *s)
{
	free(s->buf);
	s->buf=NULL;
	s->l=s->i=0;
}

void fputshort(unsigned short v, FILE *fp)
{
	fputc(v>>8, fp);
	fputc(v, fp);
}

void fputlong(unsigned long v, FILE *fp)
{
	fputc(v>>24, fp);
	fputc(v>>16, fp);
	fputc(v>>8, fp);
	fputc(v, fp);
}

unsigned short fgetshort(FILE *fp)
{
	unsigned char a=fgetc(fp), b=fgetc(fp);
	return((a<<8)|b);
}

unsigned long fgetlong(FILE *fp)
{
	unsigned char a=fgetc(fp), b=fgetc(fp), c=fgetc(fp), d=fgetc(fp);
	return((a<<24)|(b<<16)|(c<<8)|d);
}
