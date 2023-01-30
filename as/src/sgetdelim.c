#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>

#define DEFINE_SGETDELIM_FUNC(NAME, DELIMPARAM, DELIMTEST)                                         \
	int NAME(FILE *f, DELIMPARAM, char **out, size_t *len, char *stack, size_t stacksz,            \
			 char **heap, size_t *heapsz) {                                                        \
		char *tmp, *str = stack;                                                                   \
		size_t i = 0;                                                                              \
		int c;                                                                                     \
                                                                                                   \
		if (!f || !len || !heap || !heapsz) {                                                      \
			*out = NULL;                                                                           \
			return -EINVAL;                                                                        \
		}                                                                                          \
                                                                                                   \
		if (!*heap)                                                                                \
			*heapsz = 0;                                                                           \
                                                                                                   \
		flockfile(f);                                                                              \
                                                                                                   \
		for (;;) {                                                                                 \
			if (i == stacksz) {                                                                    \
				if (*heapsz <= stacksz) {                                                          \
					*heapsz = 2 * (i + 2);                                                         \
					tmp = realloc(*heap, *heapsz);                                                 \
					if (!tmp)                                                                      \
						goto oom;                                                                  \
					*heap = tmp;                                                                   \
				}                                                                                  \
				memcpy(*heap, stack, stacksz);                                                     \
				str = *heap;                                                                       \
			} else if (i > stacksz && i == *heapsz) {                                              \
				*heapsz = 2 * (i + 2);                                                             \
				tmp = realloc(*heap, *heapsz);                                                     \
				if (!tmp)                                                                          \
					goto oom;                                                                      \
				str = *heap = tmp;                                                                 \
			}                                                                                      \
			if ((c = getc_unlocked(f)) == EOF) {                                                   \
				if (!i || !feof(f)) {                                                              \
					funlockfile(f);                                                                \
					*out = NULL;                                                                   \
					return 0;                                                                      \
				}                                                                                  \
				break;                                                                             \
			}                                                                                      \
			if (DELIMTEST)                                                                         \
				break;                                                                             \
		}                                                                                          \
		str[i] = 0;                                                                                \
                                                                                                   \
		funlockfile(f);                                                                            \
                                                                                                   \
		*len = i;                                                                                  \
		*out = str;                                                                                \
		return 0;                                                                                  \
                                                                                                   \
	oom:                                                                                           \
		funlockfile(f);                                                                            \
		*out = NULL;                                                                               \
		return -ENOMEM;                                                                            \
	}

DEFINE_SGETDELIM_FUNC(sgetdelims, char *delims, (strchr(delims, str[i++] = c)))
DEFINE_SGETDELIM_FUNC(sgetdelimp, int (*delim_p)(char), (delim_p(str[i++] = c)))
DEFINE_SGETDELIM_FUNC(sgetdelim, int delim, ((str[i++] = c) == delim))

int sgetline(FILE *f, char **out, size_t *len, char *stack, size_t stacksz, char **heap,
			 size_t *heapsz) {
	return sgetdelim(f, '\n', out, len, stack, stacksz, heap, heapsz);
}
