#include "bits.h"

typedef struct
{
	string name;
	string data;
}
kf_entry;

typedef struct
{
	unsigned short nents;
	kf_entry *ents;
}
kf_archive;

int kf_read(FILE *kfa, kf_archive *buf);
