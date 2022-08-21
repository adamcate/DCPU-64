extern int sgetdelims(FILE *f, char *delims,
	char **out, size_t *len,
	char *stack, size_t stacksz,
	char **heap, size_t *heapsz);
extern int sgetdelim_p(FILE *f, int (*delim_p)(char),
	char **out, size_t *len,
	char *stack, size_t stacksz,
	char **heap, size_t *heapsz);
extern int sgetdelim(FILE *f, int delim, char **out, size_t *len,
	char *stack, size_t stacksz,
	char **heap, size_t *heapsz);
extern int sgetline(FILE *f, char **out, size_t *len,
	char *stack, size_t stacksz,
	char **heap, size_t *heapsz);
