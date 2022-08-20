#include <assert.h>
#include <ctype.h>
#include <locale.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "printf.h"
#include "optparse.h"
#include "memory.h"

#ifndef NDEBUG
#define INFO(...) do { if (opts->verbose >= 1) { efprintf(stderr, __VA_ARGS__); } } while (0)
#else
#define INFO(...) do { } while (0)
#endif

#ifndef NDEBUG
#define TRACE(...) do { if (opts->verbose >= 2) { efprintf(stderr, __VA_ARGS__); } } while (0)
#else
#define TRACE(...) do { } while (0)
#endif

struct asopts {
	char *output;
	int verbose;
	bool dontlink;
	bool error;
};

static
int
getlines(FILE *stream, char **buf, size_t *len, size_t *size)
{
	ssize_t slen;

	if ((slen = getline(buf, size, stream)) == -1L)
		return -1;

	*len = (size_t)slen;

	assert((*buf)[*len] == 0);
	assert((*buf)[*len-1] == '\n');
	(*buf)[*len-1] = 0;
	(*len)--;

	return 0;
}

static char opers[] = "+-*/%[]()~&|^.,:";

enum token_type {
	TT_ERROR,
	TT_IDENT,
	TT_DECINT,
	TT_HEXINT,
	TT_OCTINT,
	TT_DECINTK,
	TT_HEXINTK,
	TT_OCTINTK,
	TT_KEYWORD,
	TT_COMMENT,
	TT_PLUS = '+',
	TT_MINUS = '-',
	TT_STAR = '*',
	TT_SLASH = '/',
	TT_PCENT = '%',
	TT_LSQ = '[',
	TT_RSQ = '[',
	TT_LBRACK = '(',
	TT_RBRACK = ')',
	TT_TILDE = '~',
	TT_AMPER = '&',
	TT_PIPE = '|',
	TT_CARET = '^',
	TT_DOT = '.',
	TT_COMMA = ',',
	TT_COLON = ':',
};

struct token {
	size_t line, col;
	const char *str;
	size_t len;
	int type;
};

static
void
consume_ws(const char *buf, size_t len, size_t *offset)
{
	while (*offset < len) {
		if (!isspace(buf[*offset]))
			break;
		(*offset)++;
	}
}

static
bool
isident(int c)
{
	return isalpha(c) || c == '_' || c == '.';
}

static
bool
isidnum(int c)
{
	return isalnum(c) || c == '_' || c == '.';
}

static
int
consume(struct asopts *opts, const char *buf, size_t len, size_t *offset, struct token *tok)
{
	consume_ws(buf, len, offset);
	/* TRACE("ch=%d\n", (int)buf[*offset]); */

	if (!buf[*offset])
		return 1;

	if (buf[*offset] == ';') {
		tok->type = TT_COMMENT;
		tok->str = buf + *offset;
		tok->len = len - *offset;
		*offset = len;
		return 0;
	}

	const char *chr;
	if ((chr = strchr(opers, buf[*offset])) != NULL) {
		tok->type = *chr;
		tok->str = buf + *offset;
		tok->len = 1;
		(*offset)++;
		return 0;
	}

	size_t offset_before = *offset;
	if (isident(buf[*offset])) {
		(*offset)++;
		while (isidnum(buf[*offset]))
			(*offset)++;

		tok->type = TT_IDENT;
		tok->str = buf + offset_before;
		tok->len = *offset - offset_before;
		return 0;
	}

	if (buf[*offset] == '0') {
		(*offset)++;
		if (buf[*offset] == 'x') {
			(*offset)++;

			while (isdigit(buf[*offset]))
				(*offset)++;

			if (buf[*offset] == 'k') {
				(*offset)++;
				tok->type = TT_HEXINTK;
			} else {
				tok->type = TT_HEXINT;
			}
			tok->str = buf + offset_before;
			tok->len = *offset - offset_before;
			return 0;
		}

		if (buf[*offset] == 'o')
			(*offset)++;

		while (isdigit(buf[*offset]))
			(*offset)++;

		if (buf[*offset] == 'k') {
			(*offset)++;
			tok->type = TT_OCTINTK;
		} else {
			tok->type = TT_OCTINT;
		}
		tok->str = buf + offset_before;
		tok->len = *offset - offset_before;
		return 0;
	}

	if (isdigit(buf[*offset])) {
		(*offset)++;

		while (isdigit(buf[*offset]))
			(*offset)++;

		if (buf[*offset] == 'k') {
			(*offset)++;
			tok->type = TT_DECINTK;
		} else {
			tok->type = TT_DECINT;
		}
		tok->str = buf + offset_before;
		tok->len = *offset - offset_before;
		return 0;
	}

	opts->error = true;
	tok->type = TT_ERROR;
	tok->str = buf + *offset;
	tok->len = 1;
	(*offset)++;
	return 1;
}

static
int
process(struct asopts *opts, FILE *f)
{
	char *buf = NULL;
	size_t len = 0, size = 0, line = 0;

	while (!getlines(f, &buf, &len, &size)) {
		line++;

		TRACE("LINE=%s\n", buf);

		size_t i = 0;
		struct token tok = { .line=line, .col=i };

		int res;
		while (!(res = consume(opts, buf, len, &i, &tok))) {
			INFO("(%d:\"%.*s\":%d:%d:%d)\n",
				tok.type, tok.len, tok.str, tok.len,
				tok.line, tok.col);
		}


	}

	free(buf);
	return 0;
}

static
int
master(struct asopts *opts, char **argv)
{
	char *buf;
	size_t size, len;

	if (!*argv) {
		TRACE("INPUT=-\n");

		process(opts, stdin);
	} else for (; *argv; argv++) {
		TRACE("INPUT=%s\n", *argv);

		FILE *f = fopen(*argv, "r");
		if (!f) {
			perror("fopen");
			return 1;
		}

		int result = process(opts, f);
		fclose(f);
		if (result) {
			efprintf(stderr, "an error occurred in %s\n", *argv);
			return result;
		}
	}

	TRACE("OUTPUT=%s\n", opts->output ? opts->output : "-");

	return 0;
}

__attribute__((noreturn))
static
void
usage(const char *progname, const char *errmsg)
{
	efprintf(stderr, "usage: %s [-chv] [-o OUTPUT] INPUT...", progname);
	if (errmsg)
		efprintf(stderr, ": %s", errmsg);
	efprintf(stderr, "\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	struct optparse options;
	struct asopts asopts = {0};
	int option;

	assert(setlocale(LC_ALL, ""));
	assert(setlocale(LC_COLLATE, "C.utf8"));
	assert(setlocale(LC_CTYPE, "C.utf8"));
	assert(setlocale(LC_NUMERIC, "C.utf8"));

	optparse_init(&options, argv);
	options.permute = 0;

	while ((option = optparse(&options, "cvho:")) != -1) {
	switch (option) {
		case 'v': asopts.verbose++; break;
		case 'c': asopts.dontlink = true; break;
		case 'o': asopts.output = options.optarg; break;
		case 'h': usage(argv[0], NULL);
		case '?': usage(argv[0], options.errmsg);
	}
	}
	if (asopts.verbose >= 2)
		efprintf(stderr, "VERBOSE=%d\n", asopts.verbose);

	return master(&asopts, argv + options.optind);
}
