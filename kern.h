#include <stdio.h>
#include <stdbool.h>
#include "bits.h"
typedef struct _kern KERN;
KERN *kern_init(FILE *fp, bool guess);
KERN *kern_init_s(string s, bool guess);
int kern(const char *str, signed char *dev, const KERN *k);
