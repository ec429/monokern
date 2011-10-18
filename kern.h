#include <stdio.h>
typedef struct _kern KERN;
KERN kern_init(FILE *fp);
int kern(const char *str, signed char *dev, const KERN k);
