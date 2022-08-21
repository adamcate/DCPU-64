#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>

#include "optparse.h"
#include "printf.h"
#include "memory.h"
#include "sgetdelim.h"

/**********/
/* MACROS */
/**********/

#ifndef NDEBUG
#define INFO(...) do {                  \
	if (state->opts.verbose >= 1) { \
		errf(__VA_ARGS__);      \
	}                               \
} while (0)
#else
#define INFO(...) do {} while (0)
#endif

#ifndef NDEBUG
#define TRACE(...) do {                 \
	if (state->opts.verbose >= 2) { \
		errf(__VA_ARGS__);      \
	}                               \
} while (0)
#else
#define TRACE(...) do { } while (0)
#endif

#define ARRAY_SIZE(a) ((sizeof (a)) / (sizeof (a[0])))

/*********/
/* TYPES */
/*********/

struct expr;

struct as_opts {
	char *output;
	char optimise;
	int verbose;
	bool dontlink;
	bool error;
	bool extended;
};

struct as_state {
	struct as_opts opts;
	uint16_t image[65536];
	uint16_t gaps[65536];
	uint16_t refloc[65536];
	struct expr *reftgt[65536];
	const char *lblnam[65536];
	uint16_t lbltgt[65536];
};

enum token_type {
	TT_ERROR,
	TT_IDENT,
	TT_DECINT,
	TT_HEXINT,
	TT_OCTINT,
	TT_DECINTK,
	TT_HEXINTK,
	TT_OCTINTK,
	TT_OP,
	TT_KW,
	TT_EOP,
	TT_COMMENT,
	TT_PLUS = '+',
	TT_MINUS = '-',
	TT_STAR = '*',
	TT_SLASH = '/',
	TT_PCENT = '%',
	TT_LSQ = '[',
	TT_RSQ = ']',
	TT_LBRACK = '(',
	TT_RBRACK = ')',
	TT_TILDE = '~',
	TT_AMPER = '&',
	TT_PIPE = '|',
	TT_CARET = '^',
	TT_COMMA = ',',
	TT_COLON = ':',
};

struct token {
	size_t line, col;
	char *str;
	size_t len;
	int type;
	int subtype;
};

struct expr {
	int value;
	struct token tok;
	struct expr *left, *right;
};

typedef struct expr *prefix_fn(struct as_state *state, size_t line, char *buf,
	size_t len, size_t *offset, struct token *nexttok);
typedef struct expr *infix_fn(struct as_state *state, size_t line, char *buf,
	size_t len, size_t *offset, struct token *nexttok, struct expr *left);

struct pratt_entry {
	prefix_fn *prefix;
	infix_fn *infix;
	int precedence;
	bool right_assoc;
};

enum pratt_prec {
	PREC_NONE,
	PREC_TERM,
	PREC_FACTOR,
	PREC_BOR,
	PREC_XOR,
	PREC_AND,
	PREC_UNARY,
	PREC_MAX
};

/***************/
/* STATIC DATA */
/***************/

static char opers[] = "+-*/%[]()~&|^,:";
static char kws[][5] = {
	"a", "b", "c", "x", "y", "z", "i", "j", "sp", "pc", "ex", "ia",
	"push", "pop", "peek", "pick",
};
static char ops[][5] = {
	"set", "add", "sub", "mul", "mli", "div", "dvi", "mod", "mdi",
	"and", "bor", "xor", "shr", "asr", "shl",
	"ifb", "ifc", "ife", "ifn", "ifg", "ifa", "ifl", "ifu",
	"adx", "sbx", "sti", "std",
	"jsr", "int", "iag", "ias", "rfi", "iaq", "hwn", "hwq", "hwi",
	"dat", "fill",
};
static char eops[][5] = {
	"mbg", "mbo", "grm", "drm", "srt",
};

/****************/
/* OBJECT POOLS */
/****************/

static struct expr expr_pool[65536] = {0};
static int expr_ctr = 0;

/*********************/
/* UTILITY FUNCTIONS */
/*********************/

static
void
errf(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	evfprintf(stderr, fmt, args);
	va_end(args);
}

static
int
getlines(FILE *stream, char **out, size_t *len, char *stack, size_t stacksz,
		char **heap, size_t *heapsz)
{
	assert(!sgetline(stream, out, len, stack, stacksz, heap, heapsz));

	if (*out == NULL)
		return 1;

	assert(*len >= 1);
	assert((*out)[*len] == '\0');
	assert((*out)[*len-1] == '\n');
	(*out)[*len-1] = '\0';
	(*len)--;

	return 0;
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

/***************/
/* EXPRESSIONS */
/***************/

static
struct expr *
expr_new(void)
{
	assert(expr_ctr < ARRAY_SIZE(expr_pool));
	return expr_pool + expr_ctr++;
}

static
struct expr *
reduce(struct as_state *state, struct expr *expr)
{
	if (expr->left == NULL && expr->right == NULL) {
		if (expr->value == -1)
			return expr;

		if (expr->tok.type == TT_IDENT) {

		}
	}

	expr->left = reduce(state, expr->left);
	expr->right = reduce(state, expr->right);

	if (expr->left->value == -1 && expr->right->value == -1) {
		assert(0); /* not yet implemented */
	}

	return expr;
}

static
struct expr *
expr_fromtoken(struct token tok)
{
	struct expr *expr = expr_new();

	expr->value = -1;
	expr->tok = tok;
	expr->left = expr->right = NULL;

	return expr;
}

static
struct expr *
expr_fromvalue(uint16_t value)
{
	struct expr *expr = expr_new();

	expr->value = value;
	memset(&expr->tok, 0, sizeof expr->tok);
	expr->left = expr->right = NULL;

	return expr;
}

static
void
print_expr(struct expr *expr)
{
	if (expr->value == -1) {
		if (expr->left == NULL) {
			eprintf("%.*s", expr->tok.len, expr->tok.str);
			if (expr->right != NULL) {
				print_expr(expr->right);
			}
			if (expr->tok.type == TT_LSQ)
				eprintf("]");
			else if (expr->tok.type == TT_LBRACK)
				eprintf(")");
		} else {
			eprintf("(");
			if (expr->left)
				print_expr(expr->left);
			else
				eprintf("(nil)");
			eprintf(" %.*s(%d) ", expr->tok.len, expr->tok.str,
				expr->tok.type);
			if (expr->right)
				print_expr(expr->right);
			else
				eprintf("(nil)");
			eprintf(")");
		}
	} else {
		eprintf("%d", expr->value);
	}
}

/**********************/
/* SCANNING + PARSING */
/**********************/

static
void
convertkw(struct token *tok, int toktype, char (*kws)[5], size_t nkws)
{
	for (size_t i = 0; i < nkws; i++) {
		if (tok->len != strlen(kws[i]))
			continue;

		if (!strncasecmp(tok->str, kws[i], tok->len)) {
			tok->type = toktype;
			tok->subtype = i;
			return;
		}
	}
}

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
int
consume(struct as_state *state, char *buf, size_t len, size_t *offset,
		struct token *tok)
{
	consume_ws(buf, len, offset);

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

		convertkw(tok, TT_KW, kws, ARRAY_SIZE(kws));
		convertkw(tok, TT_OP, ops, ARRAY_SIZE(ops));
		if (state->opts.extended)
			convertkw(tok, TT_EOP, eops, ARRAY_SIZE(eops));

		return 0;
	}

	if (buf[*offset] == '0') {
		(*offset)++;
		if (buf[*offset] == 'x') {
			(*offset)++;

			while (isxdigit(buf[*offset]))
				(*offset)++;

			if (isalnum(buf[*offset]) && !(buf[*offset] == 'k')) {
				(*offset)++;
				goto error;
			}

			if (buf[*offset] == 'k') {
				(*offset)++;
				tok->type = TT_HEXINTK;
			} else {
				tok->type = TT_HEXINT;
			}
			tok->str = buf + offset_before;
			tok->len = *offset - offset_before;
			TRACE("HEXINT%s=%.*s\n",
				tok->type == TT_HEXINTK ? "k" : "",
				tok->len, tok->str);
			return 0;
		}

		if (buf[*offset] == 'o')
			(*offset)++;

		while (isdigit(buf[*offset]) && !(buf[*offset] == '8' ||
				buf[*offset] == '9'))
			(*offset)++;

		if (isalnum(buf[*offset]) && !(buf[*offset] == 'k')) {
			(*offset)++;
			goto error;
		}

		if (buf[*offset] == 'k') {
			(*offset)++;
			tok->type = TT_OCTINTK;
		} else {
			tok->type = TT_OCTINT;
		}
		tok->str = buf + offset_before;
		tok->len = *offset - offset_before;
		TRACE("OCTINT%s=%.*s\n",
			tok->type == TT_OCTINTK ? "k" : "",
			tok->len, tok->str);
		return 0;
	}

	if (isdigit(buf[*offset])) {
		(*offset)++;

		while (isdigit(buf[*offset]))
			(*offset)++;

		if (isalnum(buf[*offset]) && !(buf[*offset] == 'k')) {
			(*offset)++;
			goto error;
		}

		if (buf[*offset] == 'k') {
			(*offset)++;
			tok->type = TT_DECINTK;
		} else {
			tok->type = TT_DECINT;
		}
		tok->str = buf + offset_before;
		tok->len = *offset - offset_before;
		TRACE("DECINT%s=%.*s\n",
			tok->type == TT_DECINTK ? "k" : "",
			tok->len, tok->str);
		return 0;
	}

	(*offset)++;
error:
	state->opts.error = true;
	tok->type = TT_ERROR;
	tok->str = buf + offset_before;
	tok->len = *offset - offset_before;
	TRACE("ERROR=%.*s\n", tok->len, tok->str);
	return 1;
}

static
struct token
get_token(struct as_state *state, size_t line, char *buf, size_t len,
		size_t *offset)
{
	struct token tok = { .line=line, .col=*offset };

	while (consume(state, buf, len, offset, &tok) && *offset < len) {
		errf("%d:%d: unrecognised token: `%.*s'\n",
			line, *offset, tok.len, tok.str);
	}

	TRACE("TOKEN=(%d(%d):\"%.*s\":%d:%d:%d)\n",
		tok.type, tok.subtype,
		tok.len, tok.str, tok.len,
		tok.line, tok.col);

	return tok;
}

static
uint16_t
strtou16(char *str, size_t len, int base)
{
	char tmp = str[len];
	str[len] = '\0';
	errno = 0;
	long value = strtol(str, NULL, base);
	if (value < INT16_MIN)
		goto invalid;
	if (value > UINT16_MAX)
		goto invalid;
	if (value < 0)
		value += UINT16_MAX;
	goto end;
invalid:
	errf("Invalid integer literal `%s': must be in range [%d, %d]\n",
		str, INT16_MIN, UINT16_MAX);
	value = 0;
end:
	str[len] = tmp;
	return value;
}

static
struct expr *
parse_prec(struct as_state *state, size_t line, char *buf, size_t len,
		size_t *offset, struct token *tok, int prec);

static
struct expr *
decint(struct as_state *state, size_t line, char *buf,
	size_t len, size_t *offset, struct token *tok)
{
	uint16_t value = strtou16(tok->str, tok->len, 10);
	*tok = get_token(state, line, buf, len, offset);
	return expr_fromvalue(value);
}

static
struct expr *
hexint(struct as_state *state, size_t line, char *buf,
	size_t len, size_t *offset, struct token *tok)
{
	uint16_t value = strtou16(tok->str, tok->len, 16);
	*tok = get_token(state, line, buf, len, offset);
	return expr_fromvalue(value);
}

static
struct expr *
octint(struct as_state *state, size_t line, char *buf,
	size_t len, size_t *offset, struct token *tok)
{
	uint16_t value = strtou16(tok->str, tok->len, 8);
	*tok = get_token(state, line, buf, len, offset);
	return expr_fromvalue(value);
}

static
struct expr *
decintk(struct as_state *state, size_t line, char *buf,
	size_t len, size_t *offset, struct token *tok)
{
	uint16_t value = strtou16(tok->str, tok->len, 10);
	*tok = get_token(state, line, buf, len, offset);
	return expr_fromvalue(value * 1024);
}

static
struct expr *
hexintk(struct as_state *state, size_t line, char *buf,
	size_t len, size_t *offset, struct token *tok)
{
	uint16_t value = strtou16(tok->str, tok->len, 16);
	*tok = get_token(state, line, buf, len, offset);
	return expr_fromvalue(value * 1024);
}

static
struct expr *
octintk(struct as_state *state, size_t line, char *buf,
	size_t len, size_t *offset, struct token *tok)
{
	uint16_t value = strtou16(tok->str, tok->len, 8);
	*tok = get_token(state, line, buf, len, offset);
	return expr_fromvalue(value * 1024);
}

static
struct expr *
keyword(struct as_state *state, size_t line, char *buf,
	size_t len, size_t *offset, struct token *tok)
{
	struct expr *expr = expr_fromtoken(*tok);
	*tok = get_token(state, line, buf, len, offset);
	return expr;
}

static
struct expr *
ident(struct as_state *state, size_t line, char *buf,
	size_t len, size_t *offset, struct token *tok)
{
	struct expr *expr = expr_fromtoken(*tok);
	*tok = get_token(state, line, buf, len, offset);
	return expr;
}

static
struct expr *
square(struct as_state *state, size_t line, char *buf,
	size_t len, size_t *offset, struct token *tok)
{
	struct token brack = *tok;
	*tok = get_token(state, line, buf, len, offset);
	struct expr *inner = parse_prec(state, line, buf, len, offset, tok,
		PREC_TERM);
	if (tok->type == TT_RSQ) {
		*tok = get_token(state, line, buf, len, offset);
	} else {
		errf("%s:%s: unexpected `%.*s', expected `]'\n",
			line, *offset, tok->len, tok->str);
	}
	struct expr *outer = expr_fromtoken(brack);
	outer->right = inner;
	return outer;
}

static
struct expr *
grouping(struct as_state *state, size_t line, char *buf,
	size_t len, size_t *offset, struct token *tok)
{
	*tok = get_token(state, line, buf, len, offset);
	struct expr *expr = parse_prec(state, line, buf, len, offset, tok,
		PREC_TERM);
	if (tok->type == TT_RBRACK) {
		*tok = get_token(state, line, buf, len, offset);
	} else {
		errf("%s:%s: unexpected `%.*s', expected `]'\n",
			line, *offset, tok->len, tok->str);
	}
	return expr;
}

static struct pratt_entry pratt_table[128];

static
struct expr *
unary(struct as_state *state, size_t line, char *buf,
	size_t len, size_t *offset, struct token *tok)
{
	struct token op = *tok;
	struct expr *inner = parse_prec(state, line, buf, len, offset, tok,
		PREC_UNARY);
	struct expr *outer = expr_fromtoken(op);
	outer->right = inner;
	return outer;
}

static
struct expr *
binary(struct as_state *state, size_t line, char *buf,
	size_t len, size_t *offset, struct token *tok, struct expr *left)
{
	struct token op = *tok;
	*tok = get_token(state, line, buf, len, offset);
	int next_prec = pratt_table[op.type].precedence;
	if (!pratt_table[op.type].right_assoc)
		next_prec++;
	struct expr *inner = parse_prec(state, line, buf, len, offset, tok,
		next_prec);
	struct expr *outer = expr_fromtoken(op);
	outer->left = left;
	outer->right = inner;
	return outer;
}

static struct pratt_entry pratt_table[] = {
	[TT_ERROR]   = {NULL,     NULL,    PREC_NONE,    false},
	[TT_IDENT]   = {ident,    NULL,    PREC_NONE,    false},
	[TT_DECINT]  = {decint,   NULL,    PREC_NONE,    false},
	[TT_HEXINT]  = {hexint,   NULL,    PREC_NONE,    false},
	[TT_OCTINT]  = {octint,   NULL,    PREC_NONE,    false},
	[TT_DECINTK] = {decintk,  NULL,    PREC_NONE,    false},
	[TT_HEXINTK] = {hexintk,  NULL,    PREC_NONE,    false},
	[TT_OCTINTK] = {octintk,  NULL,    PREC_NONE,    false},
	[TT_OP]      = {NULL,     NULL,    PREC_NONE,    false},
	[TT_KW]      = {keyword,  NULL,    PREC_NONE,    false},
	[TT_EOP]     = {NULL,     NULL,    PREC_NONE,    false},
	[TT_COMMENT] = {NULL,     NULL,    PREC_NONE,    false},
	[TT_PLUS]    = {NULL,     binary,  PREC_TERM,    false},
	[TT_MINUS]   = {NULL,     binary,  PREC_TERM,    false},
	[TT_STAR]    = {NULL,     binary,  PREC_FACTOR,  false},
	[TT_SLASH]   = {NULL,     binary,  PREC_FACTOR,  false},
	[TT_PCENT]   = {NULL,     binary,  PREC_FACTOR,  false},
	[TT_LSQ]     = {square,   NULL,    PREC_NONE,    false},
	[TT_RSQ]     = {NULL,     NULL,    PREC_NONE,    false},
	[TT_LBRACK]  = {grouping, NULL,    PREC_NONE,    false},
	[TT_RBRACK]  = {NULL,     NULL,    PREC_NONE,    false},
	[TT_TILDE]   = {unary,    NULL,    PREC_NONE,    false},
	[TT_AMPER]   = {NULL,     binary,  PREC_AND,     false},
	[TT_PIPE]    = {NULL,     binary,  PREC_BOR,     false},
	[TT_CARET]   = {NULL,     binary,  PREC_XOR,     false},
	[TT_COMMA]   = {NULL,     NULL,    PREC_NONE,    false},
	[TT_COLON]   = {NULL,     NULL,    PREC_NONE,    false},
};

static
struct expr *
parse_prec(struct as_state *state, size_t line, char *buf, size_t len,
		size_t *offset, struct token *tok, int prec)
{
	TRACE("parse_prec(prec=%d)\n", prec);
	prefix_fn *pfn = pratt_table[tok->type].prefix;
	if (!pfn) {
		errf("Unexpected `%.*s', expected expression.\n",
			tok->len, tok->str);
		return NULL;
	}

	struct expr *expr = pfn(state, line, buf, len, offset, tok);
	assert(expr);

	struct pratt_entry *entry = &pratt_table[tok->type];
	while (prec <= entry->precedence) {
		infix_fn *ifn = entry->infix;
		assert(ifn);
		expr = ifn(state, line, buf, len, offset, tok, expr);
		assert(expr);
		entry = &pratt_table[tok->type];
	}

	return expr;
}

static
int
consume_line(struct as_state *state, size_t line, char *buf, size_t len)
{
	struct token tok;
	size_t i = 0;

	tok = get_token(state, line, buf, len, &i);

	while (tok.type == TT_IDENT) {
		struct token label = tok;

		tok = get_token(state, line, buf, len, &i);
		if (tok.type != TT_COLON) {
			errf("%d:%d: unexpected `%.*s' (expected `:').\n",
				line, i, tok.len, tok.str);
			return 1;
		}

		INFO("add `%.*s' to the label table\n", label.len, label.str);
		tok = get_token(state, line, buf, len, &i);
	}

	if (tok.type == TT_OP || tok.type == TT_EOP) {
		struct token op = tok;

		tok = get_token(state, line, buf, len, &i);
		struct expr *left = parse_prec(state, line, buf, len, &i, &tok,
				PREC_TERM);
		if (left == NULL)
			return 1;

		if (state->opts.verbose >= 2) {
			eprintf("LEFT=");
			print_expr(left);
			eprintf("\n");
		}

		if (tok.type == TT_COMMA) {
			tok = get_token(state, line, buf, len, &i);
			struct expr *right = parse_prec(state, line, buf, len,
					&i, &tok, PREC_TERM);
			if (right == NULL)
				return 1;

			if (state->opts.verbose >= 2) {
				eprintf("RIGHT=");
				print_expr(right);
				eprintf("\n");
			}
		}

		INFO("add `%.*s' to the image\n", op.len, op.str);
		return 0;
	}

	return 1;
}

static
int
process(struct as_state *state, FILE *f)
{
	char sbuf[100] = {0};
	char *buf = NULL, *str;
	size_t len = 0, size = 0, line = 0;

	while (!getlines(f, &str, &len, sbuf, 100, &buf, &size)) {
		line++;

		TRACE("LINE=%s\n", str);

		consume_line(state, line, str, len);
	}

	free(buf);
	return 0;
}

static
int
master(struct as_state *state, char **argv)
{
	char *buf;
	size_t size, len;

	if (!*argv) {
		TRACE("INPUT=-\n");

		process(state, stdin);
	} else for (; *argv; argv++) {
		TRACE("INPUT=%s\n", *argv);

		FILE *f = fopen(*argv, "r");
		if (!f) {
			perror("fopen");
			return 1;
		}

		int result = process(state, f);
		fclose(f);
		if (result) {
			errf("an error occurred in %s\n", *argv);
			return result;
		}
	}

	TRACE("OUTPUT=%s\n", state->opts.output ? state->opts.output : "-");

	return 0;
}

__attribute__((noreturn))
static
void
usage(const char *progname, const char *errmsg)
{
	errf("usage: %s [-chv] [-o OUTPUT] INPUT...", progname);
	if (errmsg)
		errf(": %s", errmsg);
	errf("\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	struct optparse options;
	struct as_state state = {0};
	int option;

	/* together, these four lines allocate 49 times in glibc.  WHY? */
	/* assert(setlocale(LC_ALL, "")); */
	/* assert(setlocale(LC_COLLATE, "C.utf8")); */
	/* assert(setlocale(LC_CTYPE, "C.utf8")); */
	/* assert(setlocale(LC_NUMERIC, "C.utf8")); */

	optparse_init(&options, argv);
	options.permute = 0;

	while ((option = optparse(&options, "hvceo:O:")) != -1) {
		switch (option) {
		case 'v': state.opts.verbose++; break;
		case 'c': state.opts.dontlink = true; break;
		case 'e': state.opts.extended = true; break;
		case 'o': state.opts.output   = options.optarg; break;
		case 'O': state.opts.optimise = options.optarg[0]; break;
		case 'h': usage(argv[0], NULL);
		case '?': usage(argv[0], options.errmsg);
		}
	}

	if (state.opts.verbose >= 3)
		errf("VERBOSE=%d\n", state.opts.verbose);

	return master(&state, argv + options.optind);
}
