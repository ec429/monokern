#include <stdio.h>
#include "bits.h"
typedef struct _kern KERN;
KERN *kern_init(FILE *fp);
KERN *kern_init_s(string s);
int kern(const char *str, signed char *dev, const KERN *k);
