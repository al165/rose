#include <stdio.h>

#include "m_pd.h"

void rose_maketag(void *x, const char *suffix, char *dest, size_t n)
{
	snprintf(dest, n, "%p_%s", x, suffix);
	dest[n - 1] = '\0';
}

t_float rose_clip01(t_float f)
{
	return (f < 0 ? 0 : (f > 1 ? 1 : f));
}
