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

static void rule_program(Parser *p);
static void rule_prolog(Parser *p);
static void rule_top_list(Parser *p);
static void rule_top_item(Parser *p);
static bool first_top_item(const Parser *p);
static void rule_function_def(Parser *p);
static void rule_type_def(Parser *p);
static void rule_type(Parser *p);
static bool first_type(Parser *p);
static bool first_simple_type(Parser *p);
static void rule_union_rest(Parser *p);
static void rule_simple_type(Parser *p);
static void rule_case_class_def(Parser *p);
static void rule_extension_def(Parser *p);
static void rule_method_list(Parser *p);
static void rule_method_list_rest(Parser *p);
static void rule_annotation_eol(Parser *p);
static void rule_param_lists(Parser *p);
static void rule_param(Parser *p);
static void rule_param_list(Parser *p);
static void rule_param_list_rest(Parser *p);
static void rule_optional_type(Parser *p);
static void rule_func_type_params(Parser *p);
static void rule_func_type_rest(Parser *p);
static void rule_rhs(Parser *p);
static void rule_block(Parser *p);
static void rule_statement_list(Parser *p);
static bool first_statement_line_rest(Parser *p);
static void rule_statement_list_rest(Parser *p);
static void rule_statement_line(Parser *p);
static void rule_statement_line_rest(Parser *p);
static void rule_statement(Parser *p);
static void rule_inline_statement(Parser *p);
static void rule_var_definition(Parser *p);
static void rule_var_kind(Parser *p);
static void rule_id_statement(Parser *p);
static void rule_return_statement(Parser *p);
static bool follow_return_value(Parser *p);
static void rule_return_value(Parser *p);
static void rule_if_statement(Parser *p);
static void rule_else_part(Parser *p);
static void rule_while_statement(Parser *p);
static void rule_body(Parser *p);
static void rule_for_statement(Parser *p);
static void rule_for_header(Parser *p);
static void rule_generator(Parser *p);
static void rule_range_op(Parser *p);
static void rule_step(Parser *p);
static void rule_inline_guards(Parser *p);
static void rule_guard_lines(Parser *p);
static void rule_value(Parser *p);
static void rule_match_suffix(Parser *p);
static void rule_case_list(Parser *p);
static void rule_case_list_rest(Parser *p);
static void rule_case_clause(Parser *p);
static void rule_case_guard(Parser *p);
static void rule_pattern(Parser *p);
static void rule_id_pattern(Parser *p);
static bool first_literal_pattern(Parser *p);
static bool first_subpattern(Parser *p);
static void rule_subpattern_list(Parser *p);
static void rule_subpattern_rest(Parser *p);
static void rule_subpattern(Parser *p);
static void rule_literal_pattern(Parser *p);
static void rule_number(Parser *p);
static bool fetch_token(Parser *p);

void handle_tokens(Parser *p);
bool parser_init(Parser *p, FILE *src);
void parser_free(Parser *p);
const Token *parser_peek(const Parser *p);
bool parser_check(const Parser *p, t_type type);
bool parser_advance(Parser *p);
bool parser_expect(Parser *p, t_type type, tree_node_t **out);

#endif
