#include "kfa.h"

int kf_read(FILE *kfa, kf_archive *buf)
{
	if(!kfa) return(-1);
	if(!buf) return(-1);
	buf->nents=fgetshort(kfa);
	return(buf->nents);
}
