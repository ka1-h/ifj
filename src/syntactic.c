#include <stdlib.h>
#include <stdio.h>

#include "syntactic.h"
#include "tree.h"

void handle_tokens(Parser *p){
    rule_program(p);
}

static void rule_program(Parser *p){

    rule_prolog(p);
    if (p->error != 0) return;

    rule_top_list(p);
    if (p->error != 0) return;

    rule_EOF(p);
    if (p->error != 0) return;
}

static void rule_prolog(Parser *p){
    if (!parser_expect(p, KW_IMPORT, NULL)) return;
    if (!parser_expect(p, T_IFJ, NULL)) return;
    if (!parser_expect(p, T_LF, NULL)) return;
}

static void rule_top_list(Parser *p){
    if(first_top_item(p)){
        rule_top_item(p);
        rule_top_list(p);
    }
}

static void rule_top_item(Parser *p){
    Token t = *parser_peek(p);
    if(t.type == T_MAIN){
    }else if(t.type == KW_DEF){
    }else if(t.type == KW_CLASS){
    }else if(t.type == KW_EXTENSION){
    }
}

static bool first_top_item(const Parser *p) {
    return parser_check(p, T_MAIN)   ||
            parser_check(p, KW_DEF)   ||
            parser_check(p, KW_TYPE)  ||
            parser_check(p, KW_CASE)  ||
            parser_check(p, KW_EXTENSION);
}

static void rule_function_def(Parser *p){
    if (!parser_expect(p, KW_DEF, NULL)) return;
    if (!parser_expect(p, T_ID, NULL)) return;

    rule_param_lists(p);
    rule_optional_type(p);
    
    if (!parser_expect(p, T_EQU_SIGN, NULL)) return;
    rule_rhs(p);
}

static void rule_type_def(Parser *p){
    if (!parser_expect(p, KW_TYPE, NULL)) return;
    if (!parser_expect(p, T_ID, NULL)) return;
    if (!parser_expect(p, T_EQU_SIGN, NULL)) return;

    rule_type(p);
}

static void rule_type(Parser *p){
    if(first_simple_type(p)){
        rule_simple_type(p);
        rule_union_rest(p);
    }else{
        parser_expect(p, T_LPARENTH, NULL);
        rule_func_type_params(p);
        parser_expect(p, T_RPARENTH, NULL);
        parser_expect(p, T_GOET, NULL);
        rule_type(p);
    }
}

static bool first_simple_type(Parser *p){
    if(parser_check(p, KW_BOOLEAN) ||
        parser_check(p, KW_INT) ||
        parser_check(p, KW_STRING) ||
        parser_check(p, KW_DOUBLE) ||
        parser_check(p, KW_UNIT) ||
        parser_check(p, T_ID)){
            return true;
        }
    return false;
}

static void rule_union_rest(Parser *p){
    if (parser_check(p, T_UNIF)){
        parser_expect(p, T_UNIF, NULL);
        rule_simple_type(p);
        rule_union_rest(p);
    }
}

static void rule_simple_type(Parser *p){
    if (parser_check(p, KW_BOOLEAN)) {
        parser_expect(p, KW_BOOLEAN, NULL);
        return;
    }
    if (parser_check(p, KW_INT)) {
        parser_expect(p, KW_INT, NULL);
        return;
    }
    if (parser_check(p, KW_STRING)) {
        parser_expect(p, KW_STRING, NULL);
        return;
    }
    if (parser_check(p, KW_DOUBLE)) {
        parser_expect(p, KW_DOUBLE, NULL);
        return;
    }
    if (parser_check(p, KW_UNIT)) {
        parser_expect(p, KW_UNIT, NULL);
        return;
    }
    parser_expect(p, T_ID, NULL);
}

static void rule_eol(Parser *p){

}

static void rule_case_class_def(Parser *p){

}

static void rule_extension_def(Parser *p){

}

static void rule_annotation_eol(Parser *p){

}

static void rule_param_lists(Parser *p){

}

static void rule_optional_type(Parser *p){

}

static void rule_rhs(Parser *p){

}


// Reads the next token that is not whitespace into p->current
static bool fetch_token(Parser *p) {
    do {
        int res = get_token(p->src, &p->current, &p->buffer, &p->dent, &p->processed,
                            &p->pos, p->pole_dent, &p->mul_de);
        if (res != 0) {
            free(p->current.data);
            p->current.data = NULL;
            if (p->error == PARSE_OK) {
                p->error = PARSE_ERR_LEX;
            }
            return false;
        }
        if (p->current.type == T_SPACE) {
            free(p->current.data);
            p->current.data = NULL;
        }
    } while (p->current.type == T_SPACE);
    return true;
}

bool parser_init(Parser *p, FILE *src) {
    p->src = src;
    p->dent = 0;
    p->pos = 0;
    for (int i = 0; i < 256; i++) {
        p->pole_dent[i] = 0;
    }
    p->processed = true;
    p->mul_de = false;
    p->current.type = T_EOF;
    p->current.data = NULL;
    p->current.line = 0;
    p->error = PARSE_OK;
    buffer_init(&p->buffer);

    return fetch_token(p);
}

void parser_free(Parser *p) {
    free(p->current.data);
    p->current.data = NULL;
    buffer_free(&p->buffer);
}


const Token *parser_peek(const Parser *p) {
    return &p->current;
}

bool parser_check(const Parser *p, t_type type) {
    return p->current.type == type;
}

bool parser_advance(Parser *p) {
    if (p->error != PARSE_OK) {
        return false;
    }
    if (p->current.type == T_EOF) {
        return true;    
    }
    free(p->current.data);
    p->current.data = NULL;
    return fetch_token(p);
}

bool parser_expect(Parser *p, t_type type, tree_node_t **out) {
    if (p->error != PARSE_OK) {
        return false;
    }
    if (p->current.type != type) {
        p->error = PARSE_ERR_SYNTAX;
        return false;
    }

    if (out) {
        *out = tree_create_terminal(&p->current);
        if (!*out) {
            p->error = PARSE_ERR_INTERNAL;
            return false;
        }
    }

    if (!parser_advance(p)) {
        if (out) {
            tree_dispose(out);
        }
        return false;
    }
    return true;
}