#ifndef SYNTACTIC_H
#define SYNTACTIC_H

#include <stdbool.h>
#include <stdio.h>

#include "scanner.h"
#include "tree.h"

// Exit codes reported through Parser.error 
#define PARSE_OK         0
#define PARSE_ERR_LEX    1
#define PARSE_ERR_SYNTAX 2
#define PARSE_ERR_INTERNAL 99


// Parser = scanner state + one token of lookahead 
typedef struct {
    FILE *src;
    Buffer buffer;
    int dent;
    int pos;
    int pole_dent[256];
    bool processed;
    bool mul_de;

    Token current;      // the lookahead token, always valid after parser_init
    int error;          // PARSE_OK until something fails; first error sticks
} Parser;

bool parser_init(Parser *p, FILE *src);
void parser_free(Parser *p);
const Token *parser_peek(const Parser *p);
bool parser_check(const Parser *p, t_type type);
bool parser_advance(Parser *p);
bool parser_expect(Parser *p, t_type type, tree_node_t **out);

#endif
