#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#define MAX_CFUNCTIONS 200
#define MAX_VARIABLES 1000

enum {
	LIST,
	STRING,
	SYMBOL,
	INTEGER,
	FLOAT,
	LBRACE,
	RBRACE,
	CONST_NIL,
	CONST_T,
};

typedef struct token {
	unsigned char type;
	int i;
	float f;
	char *s;
	struct token *children;
	int sz;
} Token;

typedef Token (*fptr)(Token*, int);

typedef struct cfunction {
	fptr fun;
	const char *name;
} CFunction;

typedef struct variable {
	int depth;
	char *name;
	Token token;
} Variable;

Token *gTokens = 0;
int gNumTokens = 0;

char *gStr;
int gStrSz;
int gStrMax = 120;

CFunction gCFunctions[MAX_CFUNCTIONS];
int gNumCFunctions = 0;

Variable gVariables[MAX_VARIABLES];
int gNumVariables = 0;

int gDepth = 0;

void freeToken(Token t) {
	if(t.type == LIST) {
		for(int i = 0; i < t.sz; i++)
			freeToken(t.children[i]);
		free(t.children);
		return;
	}
	if(t.type == STRING || t.type == SYMBOL)
		free(t.s);
}

void freeTokens() {
	for(int i = 0; i < gNumTokens; i++) {
		freeToken(gTokens[i]);
		printf("%d\n", i);
	}
	free(gTokens);
}

void setVariable(char *name, int depth, Token token) {
	for(int i = 0; i < gNumVariables; i++)
		if(strcmp(gVariables[i].name, name) == 0 && gVariables[i].depth == depth) {
			freeToken(gVariables[i].token);
			gVariables[i].token = token;
			return;
		}
	Variable v;
	v.name = name;
	v.depth = depth;
	v.token = token;
	gVariables[gNumVariables++] = v;
}

Token getVariable(char *name, int depth) {
	for(int d = depth; d >= 0; d--)
		for(int i = 0; i < gNumVariables; i++)
			if(strcmp(gVariables[i].name, name) == 0 && gVariables[i].depth == d)
				return gVariables[i].token;
	printf("variable %s not found\n", name);
	exit(1);
}

void freeVariables(int depth) {
	for(int i = 0; i < gNumVariables; i++)
		if(gVariables[i].depth >= depth) {
			freeToken(gVariables[i].token);
			gVariables[i--] = gVariables[--gNumVariables];
		}
}

void clearVariable(char *name) {
	for(int i = 0; i < gNumVariables; i++)
		if(strcmp(gVariables[i].name, name) == 0) {
			freeToken(gVariables[i].token);
			gVariables[i--] = gVariables[--gNumVariables];
		}
}

void addCFunction(const char *name, fptr fun) {
	gCFunctions[gNumCFunctions++] = (CFunction){fun, name};
}

void strAppend(char c) {
	gStr[gStrSz++] = c;
	gStr[gStrSz] = 0;
	if(gStrSz > gStrMax-10) {
		gStrMax += 20;
		gStr = realloc(gStr, gStrMax);
	}
}

#define strClear() gStrSz = 0

char *strCopy() {
	char *str = malloc(gStrSz+1);
	for(int i = 0; i < gStrSz; i++)
		str[i] = gStr[i];
	str[gStrSz] = 0;
	return str;
}

char *strGet() {
	char *str = strCopy();
	gStrSz = 0;
	return str;
}

char *strGetP() {
	gStrSz = 0;
	return gStr;
}

void strBegin() {
	gStr = malloc(gStrMax);
	gStrSz = 0;
}

void strEnd() {
	free(gStr);
}

void addToken(Token t) {
	gTokens = realloc(gTokens, sizeof(Token)*(++gNumTokens));
	gTokens[gNumTokens-1] = t;
}

void addSymbol(char *s) {
	Token t;
	t.type = SYMBOL;
	t.s = s;
	addToken(t);
}

void addString(char *s) {
	Token t;
	t.type = STRING;
	t.s = s;
	addToken(t);
}

void addInteger(int i) {
	Token t;
	t.type = INTEGER;
	t.i = i;
	addToken(t);
}

void addFloat(float f) {
	Token t;
	t.type = FLOAT;
	t.f = f;
	addToken(t);
}

void addType(unsigned char type) {
	Token t;
	t.type = type;
	addToken(t);
}

#define addLBrace() addType(LBRACE)
#define addRBrace() addType(RBRACE)

void addList(Token *children, int sz) {
	Token t;
	t.type = LIST;
	t.children = children;
	t.sz = sz;
	addToken(t);
}

void writeToken(Token t) {
	int i;
	switch(t.type) {
	case LIST:
		printf("(");
		for(i = 0; i < t.sz; i++) {
			writeToken(t.children[i]);
			if(i != t.sz-1)
				printf(" ");
		}
		printf(")");
		break;
	case INTEGER:
		printf("%d", t.i);
		break;
	case FLOAT:
		printf("%f", t.f);
		break;
	case SYMBOL:
		printf("%s", t.s);
		break;
	case STRING:
		printf("\"%s\"", t.s);
		break;
	case CONST_T:
		printf("T");
		break;
	case CONST_NIL:
		printf("nil");
		break;
	default:
		printf("(?)");
	}
}

bool is_int(char *s) {
	for(int i = 0; s[i]; i++) {
		if(s[i] == '-') {
			if(i != 0)
				return false;
		}
		else if(s[i] < '0' || s[i] > '9')
			return false;
	}
	return true;
}

bool is_float(char *s) {
	int p = 0;
	for(int i = 0; s[i]; i++) {
		if(s[i] == '-') {
			if(i != 0)
				return false;
		}
		else if(s[i] == '.') {
			if(p++)
				return false;
		}
		else if(s[i] < '0' || s[i] > '9')
			return false;
	}
	return true;
}

void convertLiterals() {
	for(int i = 0; i < gNumTokens; i++) {
		if(gTokens[i].type != SYMBOL)
			continue;
		if(is_int(gTokens[i].s)) {
			gTokens[i].type = INTEGER;
			gTokens[i].i = atoi(gTokens[i].s);
			free(gTokens[i].s);
		}
		else if(is_float(gTokens[i].s)) {
			gTokens[i].type = FLOAT;
			gTokens[i].f = atof(gTokens[i].s);
			free(gTokens[i].s);
		}
		else if(strcmp(gTokens[i].s, "nil") == 0) {
			gTokens[i].type = CONST_NIL;
			free(gTokens[i].s);
		}
		else if(strcmp(gTokens[i].s, "T") == 0) {
			gTokens[i].type = CONST_T;
			free(gTokens[i].s);
		}
	}
}

void evalBraces(Token *tokens, int *sz) {
	for(int i = 0; i < *sz; i++) {
		if(tokens[i].type == LBRACE) {
			int depth = 1;
			int j;
			for(j = i+1; depth > 0; j++) {
				if(tokens[j].type == LBRACE)
					depth++;
				else if(tokens[j].type == RBRACE)
					depth--;
			}
			j--;

			//printf("%d %d\n", i, j);

			int ss = j-i-1;
			Token *sub = malloc(sizeof(Token)*ss);
			for(int k = i+1; k < j; k++)
				sub[k-(i+1)] = tokens[k];
			for(int k = i+1; k < *sz - ss; k++)
				tokens[k] = tokens[k+(j-i)];

			*sz -= ss + 1;

			tokens[i].type = LIST;
			tokens[i].children = sub;
			tokens[i].sz = ss;

			evalBraces(tokens[i].children, &tokens[i].sz);
		}
	}
}

void getTokens(char *text) {
	if(gNumTokens || gTokens) {
		free(gTokens);
		gTokens = 0;
		gNumTokens = 0;
	}

	strBegin();

	bool quote = false, comment = false;
	int depth = 0;

	for(char *c = text; *c; c++) {
		if(*c == '\n') {
			if(quote) {
				printf("unterminated quote\n");
				exit(1);
			}
			if(comment)
				comment = false;


			if(gStrSz)
				addSymbol(strGet());
			return;
		}

		if(comment)
			continue;

		if(*c == '"') {
			if(quote)
				addString(strGet());
			else if(gStrSz)
				addSymbol(strGet());
			quote = !quote;
			continue;
		}

		if(quote) {
			strAppend(*c);
			continue;
		}

		if(*c == ';') {
			if(gStrSz)
				addSymbol(strGet());
			comment = true;
			continue;
		}

		if(*c == ' ' || *c == '\t') {
			if(gStrSz)
				addSymbol(strGet());
			continue;
		}

		if(*c == '(') {
			if(gStrSz)
				addSymbol(strGet());
			addLBrace();
			depth++;
			continue;
		}

		if(*c == ')') {
			if(gStrSz)
				addSymbol(strGet());
			addRBrace();
			depth--;
			continue;
		}

		strAppend(*c);
	}

	strEnd();

	if(quote) {
		printf("unterminated quote\n");
		exit(1);
	}

	if(depth > 0) {
		printf("unterminated brace\n");
		exit(1);
	}

	if(depth < 0) {
		printf("excess closing braces\n");
		exit(1);
	}

	convertLiterals();

	evalBraces(gTokens, &gNumTokens);
}

Token doFunction(char *s, Token *argv, int argc) {
	for(int i = 0; i < gNumCFunctions; i++)
		if(strcmp(s, gCFunctions[i].name) == 0)
			return gCFunctions[i].fun(argv, argc);
	printf("%s is not a function\n", s);
	exit(1);
}

Token eval(Token l) {
	if(l.type != LIST) {
		printf("cannot evaluate non-list\n");
		exit(1);
	}

	Token *tokens = malloc(l.sz*sizeof(Token));

	for(int i = 0; i < l.sz; i++) {
		tokens[i] = l.children[i];
	}

	if(tokens->type != SYMBOL) {
		writeToken(*tokens);
		printf(" is not a symbol\n");
		exit(1);
	}

	Token t = doFunction(tokens->s, tokens+1, l.sz-1);
	free(tokens);
	return t;
}

void evalSymbols(Token *tokens, int sz) {
	for(int i = 0; i < sz; i++) {
		Token *t = &tokens[i];
		if(t->type == SYMBOL)
			*t = getVariable(t->s, gDepth);
	}
}

void evalLists(Token *tokens, int sz) {
	for(int i = 0; i < sz; i++) {
		Token *t = &tokens[i];
		if(t->type == LIST)
			*t = eval(*t);
	}
}

Token fWriteLine(Token *args, int n) {
	assert(n == 1);
	evalSymbols(args, n);
	assert(args->type == STRING);
	printf("%s\n", args->s);
	return *args;
}

Token fSetq(Token *args, int n) {
	assert(n == 2);
	assert(args[0].type == SYMBOL);
	evalLists(args+1, 1);
	evalSymbols(args+1, 1);
	clearVariable(args[0].s);
	setVariable(args[0].s, 0, args[1]);
	return args[1];
}

Token fWhen(Token *args, int n) {
	assert(n == 2);
	if(args[0].type == LIST)
		args[0] = eval(args[0]);
	if(args[0].type == SYMBOL)
		args[0] = getVariable(args[0].s, gDepth);
	assert(args[0].type == CONST_NIL || args[0].type == CONST_T);
	if(args[0].type == CONST_NIL)
		return args[0];
	else
		return eval(args[1]);
}

void addFunctions() {
	addCFunction("write-line", fWriteLine);
	addCFunction("setq", fSetq);
	addCFunction("when", fWhen);
}

int main() {
	addFunctions();

	getTokens("\
(setq hellomsg \"Hello, world!\")\
(setq write T)\
(when write (write-line hellomsg))");

	for(int i = 0; i < gNumTokens; i++) {
		writeToken(gTokens[i]);
		printf("\n");
	}

	printf("\n");

	for(int i = 0; i < gNumTokens; i++)
		eval(gTokens[i]);

	//freeTokens();
	freeVariables(0);
	return 0;
}
