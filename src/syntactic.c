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

    parser_expect(p, T_EOF, NULL);
}

static void rule_prolog(Parser *p){
    if (!parser_expect(p, KW_IMPORT, NULL)) return;
    if (!parser_expect(p, T_IFJ, NULL)) return;
    parser_expect(p, T_LF, NULL);
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
        parser_expect(p, T_MAIN, NULL);
        rule_annotation_eol(p);
        rule_function_def(p);
    }else if(t.type == KW_DEF){
        rule_function_def(p);
    }else if(t.type == KW_TYPE){
        rule_type_def(p);
    }else if(t.type == KW_CASE){
        rule_case_class_def(p);
    }else if(t.type == KW_EXTENSION){
        rule_extension_def(p);
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
    parser_expect(p, T_LF, NULL);
}

static void rule_type_def(Parser *p){
    if (!parser_expect(p, KW_TYPE, NULL)) return;
    if (!parser_expect(p, T_ID, NULL)) return;
    if (!parser_expect(p, T_EQU_SIGN, NULL)) return;

    rule_type(p);
    parser_expect(p, T_LF, NULL);
}

static void rule_type(Parser *p){
    if(first_simple_type(p)){
        rule_simple_type(p);
        rule_union_rest(p);
    }else{
        if(parser_check(p, T_UNIT_BRACKETS)){
            parser_expect(p, T_UNIT_BRACKETS, NULL); // '(' ')' chodi zo scannera ako jeden token
        }else{
            parser_expect(p, T_LPARENTH, NULL);
            rule_func_type_params(p);
            parser_expect(p, T_RPARENTH, NULL);
        }
        parser_expect(p, T_LAMB, NULL);
        rule_type(p);
    }
}

static bool first_type(Parser *p){
    if (first_simple_type(p) || parser_check(p, T_LPARENTH) || parser_check(p, T_UNIT_BRACKETS)) return true;
    return false;
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


static void rule_case_class_def(Parser *p){
    parser_expect(p, KW_CASE, NULL);
    parser_expect(p, KW_CLASS, NULL);
    parser_expect(p, T_ID, NULL);
    if(parser_check(p, T_UNIT_BRACKETS)){
        parser_expect(p, T_UNIT_BRACKETS, NULL); // '(' ')' chodi zo scannera ako jeden token
    }else{
        parser_expect(p, T_LPARENTH, NULL);
        rule_param_list(p);
        parser_expect(p, T_RPARENTH, NULL);
    }
    parser_expect(p, T_LF, NULL);
}

static void rule_extension_def(Parser *p){
    parser_expect(p, KW_EXTENSION, NULL);
    parser_expect(p, T_LPARENTH, NULL);
    rule_param(p);
    parser_expect(p, T_RPARENTH, NULL);
    parser_expect(p, T_LF, NULL);
    parser_expect(p, T_INDENT, NULL);
    rule_method_list(p);
    parser_expect(p, T_DEDENT, NULL);
    parser_expect(p, T_LF, NULL);
}

static void rule_method_list(Parser *p){
    rule_function_def(p);
    rule_method_list_rest(p);
}

static void rule_method_list_rest(Parser *p){
    if(parser_check(p, KW_DEF)){
        rule_function_def(p);
        rule_method_list_rest(p);
    }
}

static void rule_annotation_eol(Parser *p){
    if(parser_check(p, T_LF)) parser_expect(p, T_LF, NULL);
}

static void rule_param_lists(Parser *p){
    if(parser_check(p, T_UNIT_BRACKETS)){
        parser_expect(p, T_UNIT_BRACKETS, NULL); // '(' ')' chodi zo scannera ako jeden token
        rule_param_lists(p);
        return;
    }
    if(parser_check(p, T_LPARENTH)){
        parser_expect(p, T_LPARENTH, NULL);
        rule_param_list(p);
        parser_expect(p, T_RPARENTH, NULL);
        rule_param_lists(p);
    }
}

static void rule_param(Parser *p){
    if(!parser_expect(p, T_ID, NULL)) return;
    if(!parser_expect(p, T_COL, NULL)) return;
    rule_type(p);
}

static void rule_param_list(Parser *p){
    if(parser_check(p, T_ID)){
        rule_param(p);
        rule_param_list_rest(p);
    }
}

static void rule_param_list_rest(Parser *p){
    if(parser_check(p, T_COMMA)){
        parser_expect(p, T_COMMA, NULL);
        rule_param_list(p);
    }
}

static void rule_optional_type(Parser *p){
    if(parser_check(p, T_COL)){
        parser_expect(p, T_COL, NULL);
        rule_type(p);
    }
}

static void rule_func_type_params(Parser *p){
    if(first_type(p)){
        rule_type(p);
        rule_func_type_rest(p);
    }
}

static void rule_func_type_rest(Parser *p){
    if(parser_check(p, T_COMMA)){
        parser_expect(p, T_COMMA, NULL);
        rule_type(p);
        rule_func_type_rest(p);
    }
}

static void rule_rhs(Parser *p){
    if(parser_check(p, T_LF)){
        rule_block(p);
        return;
    }

    rule_value(p);
}

static void rule_block(Parser *p){
    parser_expect(p, T_LF, NULL);
    parser_expect(p, T_INDENT, NULL);
    rule_statement_list(p);
    parser_expect(p, T_DEDENT, NULL);
}

static void rule_statement_list(Parser *p){
    rule_statement_line(p);
    rule_statement_list_rest(p);
}

static bool first_statement_line_rest(Parser *p){
    if(parser_check(p, KW_VAR) || 
        parser_check(p, KW_VAL) ||
        parser_check(p, KW_RETURN) ||
        parser_check(p, KW_IF) ||
        parser_check(p, KW_WHILE) ||
        parser_check(p, KW_FOR) ||
        parser_check(p, T_ID) ||
        parser_check(p, KW_FOR)){ //TODO expression_noid

        return true;
    }
    return false;
}

static void rule_statement_list_rest(Parser *p){
    if(first_statement_line_rest(p)){
        rule_statement_line(p);
        rule_statement_list_rest(p);
    }
}

static void rule_statement_line(Parser *p){
    rule_statement(p);
    rule_statement_line_rest(p);
}

static void rule_statement_line_rest(Parser *p){
    if(parser_check(p, T_SEMIC)){
        parser_expect(p, T_SEMIC, NULL);
        rule_statement(p);
        rule_statement_line_rest(p);
        return;
    }

    parser_expect(p, T_LF, NULL);
}

static void rule_statement(Parser *p){
    if(parser_check(p, KW_VAR) || parser_check(p, KW_VAL)){
        rule_var_definition(p);
        return;
    }

    rule_inline_statement(p);
}

static void rule_inline_statement(Parser *p){
    if(parser_check(p, KW_RETURN)){
        rule_return_statement(p);
        return;
    }
    if(parser_check(p, KW_IF)){
        rule_if_statement(p);
        return;
    }
    if(parser_check(p, KW_WHILE)){
        rule_while_statement(p);
        return;
    }
    if(parser_check(p, KW_FOR)){
        rule_for_statement(p);
        return;
    }
    if(parser_check(p, T_ID)){
        parser_expect(p, T_ID, NULL);
        rule_id_statement(p);
        return;
    }
/*    if(parser_check(p, KW_RETURN)){ TODO expression_noid 
        rule_return_statement(p);
        return;
    }*/

    //TODO finish after SA dano
}

static void rule_var_definition(Parser *p){
    rule_var_kind(p);
    parser_expect(p, T_ID, NULL);
    rule_optional_type(p);
    parser_expect(p, T_EQU_SIGN, NULL);
    rule_rhs(p);
}

static void rule_var_kind(Parser *p){
    if (parser_check(p, KW_VAR)){
        parser_expect(p, KW_VAR, NULL);
        return;
    }
    parser_expect(p, KW_VAL, NULL);
}

static void rule_id_statement(Parser *p){
    if(parser_check(p, T_EQU_SIGN)){
        parser_expect(p, T_EQU_SIGN, NULL);
        rule_rhs(p);
    }
    //TODO finish after expression after id
}


static void rule_return_statement(Parser *p){
    parser_expect(p, KW_RETURN, NULL);
    rule_return_value(p);
}

static bool follow_return_value(Parser *p){
    if(parser_check(p, T_LF) ||
        parser_check(p, T_SEMIC) ||
        parser_check(p, KW_ELSE) ||
        parser_check(p, T_COMMA) ||
        parser_check(p, T_RPARENTH)){
            return true;
        }
    return false;
}

static void rule_return_value(Parser *p){
    if(!follow_return_value(p)){
        rule_value(p);
    }
}

static void rule_if_statement(Parser *p){
    parser_expect(p, KW_IF, NULL);
    //TODO expression
    parser_expect(p, KW_THEN, NULL);
    rule_body(p);
    rule_else_part(p);
}

static void rule_else_part(Parser *p){
    if(parser_check(p, KW_ELSE)){
        parser_expect(p, KW_ELSE, NULL);
        rule_body(p);
    }
}

static void rule_while_statement(Parser *p){
    parser_expect(p, KW_WHILE, NULL);
    //TODO expression
    parser_expect(p, KW_DO, NULL);
    rule_body(p);
}

static void rule_body(Parser *p){
    if(parser_check(p, T_LF)){
        rule_block(p);
        return;
    }
    if(parser_check(p, T_MINUS) || parser_check(p, T_NEG)){
        //TODO expression_unary
        return;
    }

    rule_inline_statement(p);
}

static void rule_for_statement(Parser *p){
    parser_expect(p, KW_FOR, NULL);
    rule_for_header(p);
    parser_expect(p, KW_DO, NULL);
    rule_body(p);
}

static void rule_for_header(Parser *p){
    if(parser_check(p, T_LF)){
        parser_expect(p, T_LF, NULL);
        parser_expect(p, T_INDENT, NULL);
        rule_generator(p);
        rule_inline_guards(p);
        parser_expect(p, T_LF, NULL);
        rule_guard_lines(p);
        parser_expect(p, T_DEDENT, NULL);
        return;
    }

    rule_generator(p);
    rule_inline_guards(p);
}

static void rule_generator(Parser *p){
    parser_expect(p, T_ID, NULL);
    parser_expect(p, T_ASSIG, NULL);
    //TODO expression
    rule_range_op(p);
    //TODO expression
    rule_step(p);
}

static void rule_range_op(Parser *p){
    if(parser_check(p, KW_UNTIL)){
        parser_expect(p, KW_UNTIL, NULL);
        return;
    }

    parser_expect(p, KW_TO, NULL);
}

static void rule_step(Parser *p){
    if(parser_check(p, KW_BY)){
        parser_expect(p, KW_BY, NULL);
        //TODO expression
    }
}

static void rule_inline_guards(Parser *p){
    if(parser_check(p, KW_IF)){
        parser_expect(p, KW_IF, NULL);
        //TODO expression
        rule_inline_guards(p);
    }
}

static void rule_guard_lines(Parser *p){
    if(parser_check(p, KW_IF)){
        parser_expect(p, KW_IF, NULL);
        //TODO expression
        rule_inline_guards(p);
        parser_expect(p, T_LF, NULL);
        rule_guard_lines(p);
    }
}

static void rule_value(Parser *p){
    if(parser_check(p, KW_IF)){
        rule_if_statement(p);
        return;
    }

    //TODO value_expression
    rule_match_suffix(p);
}

static void rule_match_suffix(Parser *p){
    if(parser_check(p, KW_MATCH)){
        parser_expect(p, KW_MATCH, NULL);
        parser_expect(p, T_LF, NULL);
        parser_expect(p, T_INDENT, NULL);
        rule_case_list(p);
        parser_expect(p, T_DEDENT, NULL);
    }
}

static void rule_case_list(Parser *p){
    rule_case_clause(p);
    rule_case_list_rest(p);
}

static void rule_case_list_rest(Parser *p){
    if(parser_check(p, KW_CASE)){
        rule_case_clause(p);
        rule_case_list_rest(p);
    }
}

static void rule_case_clause(Parser *p){
    parser_expect(p, KW_CASE, NULL);
    rule_pattern(p);
    rule_case_guard(p);
    parser_expect(p, T_LAMB, NULL);
    rule_rhs(p);
    parser_expect(p, T_LF, NULL);
}

static void rule_case_guard(Parser *p){
    if(parser_check(p, KW_IF)){
        parser_expect(p, KW_IF, NULL);
        //TODO expression
    }
}

static void rule_pattern(Parser *p){
    if(parser_check(p, T_REPRESPATTERN)){
        parser_expect(p, T_REPRESPATTERN, NULL);
        return;
    }
    if(parser_check(p, T_ID)){
        parser_expect(p, T_ID, NULL);
        rule_id_pattern(p);
        return;
    }

    rule_literal_pattern(p);
}

static void rule_id_pattern(Parser *p){
    if(parser_check(p, T_COL)){
        parser_expect(p, T_COL, NULL);
        rule_type(p);
        return;
    }

    parser_expect(p, T_LPARENTH, NULL);
    rule_subpattern_list(p);
    parser_expect(p, T_RPARENTH, NULL);
}

static bool first_literal_pattern(Parser *p){
    if(parser_check(p, T_INT) ||
        parser_check(p, T_DOUBLE) ||
        parser_check(p, T_STRING) ||
        parser_check(p, KW_TRUE) ||
        parser_check(p, KW_FALSE) ||
        parser_check(p, T_MINUS) ||
        parser_check(p, T_UNIT_BRACKETS)){
            return true;
        }
    return false;
}

static bool first_subpattern(Parser *p){
    if(parser_check(p, T_ID) ||
        parser_check(p, T_REPRESPATTERN) ||
        first_literal_pattern(p)){
            return true;
        }
    return false;
}

static void rule_subpattern_list(Parser *p){
    if(first_subpattern(p)){
        rule_subpattern(p);
        rule_subpattern_rest(p);
    }
}

static void rule_subpattern_rest(Parser *p){
    if(parser_check(p, T_COMMA)){
        parser_expect(p, T_COMMA, NULL);
        rule_subpattern(p);
        rule_subpattern_rest(p);
    }
}

static void rule_subpattern(Parser *p){
    if(parser_check(p, T_ID)){
        parser_expect(p, T_ID, NULL);
        return;
    }
    if(parser_check(p, T_REPRESPATTERN)){
        parser_expect(p, T_REPRESPATTERN, NULL);
        return;
    }

    rule_literal_pattern(p);
}

static void rule_literal_pattern(Parser *p){
    if(parser_check(p, T_INT)){
        parser_expect(p, T_INT, NULL);
        return;
    }
    if(parser_check(p, T_DOUBLE)){
        parser_expect(p, T_DOUBLE, NULL);
        return;
    }
    if(parser_check(p, T_STRING)){
        parser_expect(p, T_STRING, NULL);
        return;
    }
    if(parser_check(p, KW_TRUE)){
        parser_expect(p, KW_TRUE, NULL);
        return;
    }
    if(parser_check(p, KW_FALSE)){
        parser_expect(p, KW_FALSE, NULL);
        return;
    }
    if(parser_check(p, T_MINUS)){
        parser_expect(p, T_MINUS, NULL);
        rule_number(p);
        return;
    }

    parser_expect(p, T_UNIT_BRACKETS, NULL); // '(' ')' chodi zo scannera ako jeden token
}

static void rule_number(Parser *p){
    if(parser_check(p, T_INT)){
        parser_expect(p, T_INT, NULL);
        return;
    }

    parser_expect(p, T_DOUBLE, NULL);
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