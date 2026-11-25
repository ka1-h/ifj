// project: IFJ26 compiler
// author: Daniel Čerešňa <xceresd00>

#ifndef SYNTACTIC_H
#define SYNTACTIC_H

#include <stdbool.h>
#include <stdio.h>

#include "scanner.h"
#include "tree.h"

// Exit codes reported through Parser.error
#define PARSE_OK 0
#define PARSE_ERR_LEX 1
#define PARSE_ERR_SYNTAX 2
#define PARSE_ERR_INTERNAL 99

// Parser = scanner state + one token of lookahead
typedef struct {
    FILE* src;
    Buffer buffer;
    int dent;
    int pos;
    int pole_dent[256];
    bool processed;
    bool mul_de;

    // parentheses: inside them T_LF/T_INDENT/T_DEDENT are only whitespace
    int paren_depth; // current nesting depth of '(' ')'
    int paren_base_pos; // pos in pole_dent when the outermost '(' was opened
    int paren_base_indent; // indentation of the logical line with the outermost '('
    bool paren_newline; // a physical LF was skipped inside parentheses
    bool newline_before; // a line break was skipped right before current

    t_type prev_type; // type of the last consumed token (set in parser_advance)
    Token current; // the lookahead token, always valid after parser_init
    int error; // PARSE_OK until something fails; first error sticks
} Parser;

tree_node_t* rule_program(Parser* p);
void rule_leading_eol(Parser* p, tree_node_t* parent);
void rule_line_end(Parser* p, tree_node_t* parent);
void rule_prolog(Parser* p, tree_node_t* parent);
void rule_top_list(Parser* p, tree_node_t* parent);
void rule_top_item(Parser* p, tree_node_t* parent);
bool first_top_item(const Parser* p);
void rule_function_def(Parser* p, tree_node_t* parent);
void rule_type_def(Parser* p, tree_node_t* parent);
void rule_type(Parser* p, tree_node_t* parent);
bool first_type(Parser* p);
bool first_simple_type(Parser* p);
void rule_union_rest(Parser* p, tree_node_t* parent);
void rule_simple_type(Parser* p, tree_node_t* parent);
void rule_case_class_def(Parser* p, tree_node_t* parent);
void rule_extension_def(Parser* p, tree_node_t* parent);
void rule_method_list(Parser* p, tree_node_t* parent);
void rule_method_list_rest(Parser* p, tree_node_t* parent);
void rule_annotation_eol(Parser* p, tree_node_t* parent);
void rule_param_lists(Parser* p, tree_node_t* parent);
void rule_param(Parser* p, tree_node_t* parent);
void rule_param_list(Parser* p, tree_node_t* parent);
void rule_param_list_rest(Parser* p, tree_node_t* parent);
void rule_param_after_comma(Parser* p, tree_node_t* parent);
void rule_optional_type(Parser* p, tree_node_t* parent);
void rule_func_type_params(Parser* p, tree_node_t* parent);
void rule_func_type_rest(Parser* p, tree_node_t* parent);
void rule_rhs(Parser* p, tree_node_t* parent);
void rule_block(Parser* p, tree_node_t* parent);
void rule_statement_list(Parser* p, tree_node_t* parent);
bool first_statement_line_rest(Parser* p);
bool first_expression_noid(Parser* p);
void rule_statement_list_rest(Parser* p, tree_node_t* parent);
void rule_statement_line(Parser* p, tree_node_t* parent);
void rule_statement_line_rest(Parser* p, tree_node_t* parent);
void rule_statement(Parser* p, tree_node_t* parent);
void rule_inline_statement(Parser* p, tree_node_t* parent);
void rule_var_definition(Parser* p, tree_node_t* parent);
void rule_var_kind(Parser* p, tree_node_t* parent);
void rule_id_statement(Parser* p, tree_node_t* parent);
void rule_return_statement(Parser* p, tree_node_t* parent);
bool follow_return_value(Parser* p);
void rule_return_value(Parser* p, tree_node_t* parent);
void rule_if_statement(Parser* p, tree_node_t* parent);
void rule_else_part(Parser* p, tree_node_t* parent);
void rule_while_statement(Parser* p, tree_node_t* parent);
void rule_body(Parser* p, tree_node_t* parent);
void rule_for_statement(Parser* p, tree_node_t* parent);
void rule_for_header(Parser* p, tree_node_t* parent);
void rule_generator(Parser* p, tree_node_t* parent);
void rule_range_op(Parser* p, tree_node_t* parent);
void rule_step(Parser* p, tree_node_t* parent);
void rule_inline_guards(Parser* p, tree_node_t* parent);
void rule_guard_lines(Parser* p, tree_node_t* parent);
void rule_value(Parser* p, tree_node_t* parent);
void rule_match_suffix(Parser* p, tree_node_t* parent);
void rule_case_list(Parser* p, tree_node_t* parent);
void rule_case_list_rest(Parser* p, tree_node_t* parent);
void rule_case_clause(Parser* p, tree_node_t* parent);
void rule_case_guard(Parser* p, tree_node_t* parent);
void rule_pattern(Parser* p, tree_node_t* parent);
void rule_id_pattern(Parser* p, tree_node_t* parent);
bool first_literal_pattern(Parser* p);
bool first_subpattern(Parser* p);
void rule_subpattern_list(Parser* p, tree_node_t* parent);
void rule_subpattern_rest(Parser* p, tree_node_t* parent);
void rule_subpattern(Parser* p, tree_node_t* parent);
void rule_literal_pattern(Parser* p, tree_node_t* parent);
void rule_number(Parser* p, tree_node_t* parent);
bool fetch_token(Parser* p);

tree_node_t* handle_tokens(Parser* p);
bool parser_init(Parser* p, FILE* src);
void parser_free(Parser* p);
const Token* parser_peek(const Parser* p);
bool parser_check(const Parser* p, t_type type);
bool parser_advance(Parser* p);
bool parser_expect(Parser* p, t_type type, tree_node_t* parent);

#endif
