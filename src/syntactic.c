#include <stdlib.h>
#include <stdio.h>

#include "syntactic.h"
#include "tree.h"
#include "precedence.h"

// Creates a nonterminal node and attaches it to parent; sets error 99 on failure
static tree_node_t *add_node(Parser *p, tree_node_t *parent, tree_rule_t rule){
    tree_node_t *node = tree_create_nonterminal(rule);
    if (!node || !tree_add_child(parent, node)) {
        tree_dispose(&node);
        p->error = PARSE_ERR_INTERNAL;
        return NULL;
    }
    return node;
}

tree_node_t *handle_tokens(Parser *p){
    return rule_program(p);
}

tree_node_t *rule_program(Parser *p){
    tree_node_t *node = tree_create_nonterminal(RULE_PROGRAM);
    if (!node) {
        if (p->error == PARSE_OK) p->error = PARSE_ERR_INTERNAL;
        return NULL;
    }
    if (p->error != PARSE_OK) return node;

    rule_leading_eol(p, node);
    if (p->error != 0) return node;

    rule_prolog(p, node);
    if (p->error != 0) return node;

    rule_top_list(p, node);
    if (p->error != 0) return node;

    parser_expect(p, T_EOF, node);
    return node;
}

void rule_leading_eol(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    if(parser_check(p, T_LF)){
        tree_node_t *node = add_node(p, parent, RULE_LEADING_EOL_1);
        if (!node) return;
        parser_expect(p, T_LF, node);
        rule_leading_eol(p, node);
    }else{
        add_node(p, parent, RULE_LEADING_EOL_2);
    }
}

// LINE_END := EOL | eps , no tree node
// eps only if the logical line already ended: lookahead EOF/DEDENT 
// or the previous token was DEDENT (the line ended with a block)
void rule_line_end(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    if(parser_check(p, T_LF)){
        parser_expect(p, T_LF, parent);
    }else if(!(parser_check(p, T_EOF) || parser_check(p, T_DEDENT) ||
               p->prev_type == T_DEDENT)){
        p->error = PARSE_ERR_SYNTAX;
    }
}

void rule_prolog(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node = add_node(p, parent, RULE_PROLOG);
    if (!node) return;
    if (!parser_expect(p, KW_IMPORT, node)) return;
    if (!parser_expect(p, T_IFJ, node)) return;
    rule_line_end(p, node);
}

void rule_top_list(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    if(first_top_item(p)){
        tree_node_t *node = add_node(p, parent, RULE_TOP_LIST_1);
        if (!node) return;
        rule_top_item(p, node);
        rule_top_list(p, node);
    }else{
        add_node(p, parent, RULE_TOP_LIST_2);
    }
}

void rule_top_item(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    Token t = *parser_peek(p);
    tree_node_t *node;
    if(t.type == T_MAIN){
        node = add_node(p, parent, RULE_TOP_ITEM_1);
        if (!node) return;
        parser_expect(p, T_MAIN, node);
        rule_annotation_eol(p, node);
        rule_function_def(p, node);
    }else if(t.type == KW_DEF){
        node = add_node(p, parent, RULE_TOP_ITEM_2);
        if (!node) return;
        rule_function_def(p, node);
    }else if(t.type == KW_TYPE){
        node = add_node(p, parent, RULE_TOP_ITEM_3);
        if (!node) return;
        rule_type_def(p, node);
    }else if(t.type == KW_CASE){
        node = add_node(p, parent, RULE_TOP_ITEM_4);
        if (!node) return;
        rule_case_class_def(p, node);
    }else if(t.type == KW_EXTENSION){
        node = add_node(p, parent, RULE_TOP_ITEM_5);
        if (!node) return;
        rule_extension_def(p, node);
    }
}

bool first_top_item(const Parser *p) {
    return parser_check(p, T_MAIN)   ||
            parser_check(p, KW_DEF)   ||
            parser_check(p, KW_TYPE)  ||
            parser_check(p, KW_CASE)  ||
            parser_check(p, KW_EXTENSION);
}

void rule_function_def(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node = add_node(p, parent, RULE_FUNCTION_DEF);
    if (!node) return;
    if (!parser_expect(p, KW_DEF, node)) return;
    if (!parser_expect(p, T_ID, node)) return;

    rule_param_lists(p, node);
    rule_optional_type(p, node);

    if (!parser_expect(p, T_EQU_SIGN, node)) return;
    rule_rhs(p, node);
    rule_line_end(p, node);
}

void rule_type_def(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node = add_node(p, parent, RULE_TYPE_DEF);
    if (!node) return;
    if (!parser_expect(p, KW_TYPE, node)) return;
    if (!parser_expect(p, T_ID, node)) return;
    if (!parser_expect(p, T_EQU_SIGN, node)) return;

    rule_type(p, node);
    rule_line_end(p, node);
}

void rule_type(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node;
    if(first_simple_type(p)){
        node = add_node(p, parent, RULE_TYPE_1);
        if (!node) return;
        rule_simple_type(p, node);
        rule_union_rest(p, node);
    }else{
        node = add_node(p, parent, RULE_TYPE_2);
        if (!node) return;
        if(parser_check(p, T_UNIT_BRACKETS)){
            parser_expect(p, T_UNIT_BRACKETS, node); // '(' ')' chodi zo scannera ako jeden token
        }else{
            parser_expect(p, T_LPARENTH, node);
            rule_func_type_params(p, node);
            parser_expect(p, T_RPARENTH, node);
        }
        parser_expect(p, T_LAMB, node);
        rule_type(p, node);
    }
}

bool first_type(Parser *p){
    if (first_simple_type(p) || parser_check(p, T_LPARENTH) || parser_check(p, T_UNIT_BRACKETS)) return true;
    return false;
}

bool first_simple_type(Parser *p){
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

void rule_union_rest(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    if (parser_check(p, T_UNIF)){
        tree_node_t *node = add_node(p, parent, RULE_UNION_REST_1);
        if (!node) return;
        parser_expect(p, T_UNIF, node);
        rule_simple_type(p, node);
        rule_union_rest(p, node);
    }else{
        add_node(p, parent, RULE_UNION_REST_2);
    }
}

void rule_simple_type(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node;
    if (parser_check(p, KW_INT)) {
        node = add_node(p, parent, RULE_SIMPLE_TYPE_1);
        if (node) parser_expect(p, KW_INT, node);
        return;
    }
    if (parser_check(p, KW_DOUBLE)) {
        node = add_node(p, parent, RULE_SIMPLE_TYPE_2);
        if (node) parser_expect(p, KW_DOUBLE, node);
        return;
    }
    if (parser_check(p, KW_STRING)) {
        node = add_node(p, parent, RULE_SIMPLE_TYPE_3);
        if (node) parser_expect(p, KW_STRING, node);
        return;
    }
    if (parser_check(p, KW_UNIT)) {
        node = add_node(p, parent, RULE_SIMPLE_TYPE_4);
        if (node) parser_expect(p, KW_UNIT, node);
        return;
    }
    if (parser_check(p, KW_BOOLEAN)) {
        node = add_node(p, parent, RULE_SIMPLE_TYPE_5);
        if (node) parser_expect(p, KW_BOOLEAN, node);
        return;
    }
    node = add_node(p, parent, RULE_SIMPLE_TYPE_6);
    if (node) parser_expect(p, T_ID, node);
}


void rule_case_class_def(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node = add_node(p, parent, RULE_CASE_CLASS_DEF);
    if (!node) return;
    parser_expect(p, KW_CASE, node);
    parser_expect(p, KW_CLASS, node);
    parser_expect(p, T_ID, node);
    if(parser_check(p, T_UNIT_BRACKETS)){
        parser_expect(p, T_UNIT_BRACKETS, node); // '(' ')' chodi zo scannera ako jeden token
    }else{
        parser_expect(p, T_LPARENTH, node);
        rule_param_list(p, node);
        parser_expect(p, T_RPARENTH, node);
    }
    rule_line_end(p, node);
}

void rule_extension_def(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node = add_node(p, parent, RULE_EXTENSION_DEF);
    if (!node) return;
    parser_expect(p, KW_EXTENSION, node);
    parser_expect(p, T_LPARENTH, node);
    rule_param(p, node);
    parser_expect(p, T_RPARENTH, node);
    parser_expect(p, T_LF, node);
    parser_expect(p, T_INDENT, node);
    rule_method_list(p, node);
    parser_expect(p, T_DEDENT, node);
    rule_line_end(p, node);
}

void rule_method_list(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node = add_node(p, parent, RULE_METHOD_LIST);
    if (!node) return;
    rule_function_def(p, node);
    rule_method_list_rest(p, node);
}

void rule_method_list_rest(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    if(parser_check(p, KW_DEF)){
        tree_node_t *node = add_node(p, parent, RULE_METHOD_LIST_REST_1);
        if (!node) return;
        rule_function_def(p, node);
        rule_method_list_rest(p, node);
    }else{
        add_node(p, parent, RULE_METHOD_LIST_REST_2);
    }
}

void rule_annotation_eol(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    if(parser_check(p, T_LF)){
        tree_node_t *node = add_node(p, parent, RULE_ANNOTATION_EOL_1);
        if (node) parser_expect(p, T_LF, node);
    }else{
        add_node(p, parent, RULE_ANNOTATION_EOL_2);
    }
}

void rule_param_lists(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node;
    if(parser_check(p, T_UNIT_BRACKETS)){
        node = add_node(p, parent, RULE_PARAM_LISTS_1);
        if (!node) return;
        parser_expect(p, T_UNIT_BRACKETS, node); // '(' ')' chodi zo scannera ako jeden token
        rule_param_lists(p, node);
        return;
    }
    if(parser_check(p, T_LPARENTH)){
        node = add_node(p, parent, RULE_PARAM_LISTS_1);
        if (!node) return;
        parser_expect(p, T_LPARENTH, node);
        rule_param_list(p, node);
        parser_expect(p, T_RPARENTH, node);
        rule_param_lists(p, node);
        return;
    }
    add_node(p, parent, RULE_PARAM_LISTS_2);
}

void rule_param(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node = add_node(p, parent, RULE_PARAM);
    if (!node) return;
    if(!parser_expect(p, T_ID, node)) return;
    if(!parser_expect(p, T_COL, node)) return;
    rule_type(p, node);
}

void rule_param_list(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    if(parser_check(p, T_ID)){
        tree_node_t *node = add_node(p, parent, RULE_PARAM_LIST_1);
        if (!node) return;
        rule_param(p, node);
        rule_param_list_rest(p, node);
    }else{
        add_node(p, parent, RULE_PARAM_LIST_2);
    }
}

void rule_param_list_rest(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    if(parser_check(p, T_COMMA)){
        tree_node_t *node = add_node(p, parent, RULE_PARAM_LIST_REST_1);
        if (!node) return;
        parser_expect(p, T_COMMA, node);
        rule_param_list(p, node);
    }else{
        add_node(p, parent, RULE_PARAM_LIST_REST_2);
    }
}

void rule_optional_type(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    if(parser_check(p, T_COL)){
        tree_node_t *node = add_node(p, parent, RULE_OPTIONAL_TYPE_1);
        if (!node) return;
        parser_expect(p, T_COL, node);
        rule_type(p, node);
    }else{
        add_node(p, parent, RULE_OPTIONAL_TYPE_2);
    }
}

void rule_func_type_params(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    if(first_type(p)){
        tree_node_t *node = add_node(p, parent, RULE_FUNC_TYPE_PARAMS_1);
        if (!node) return;
        rule_type(p, node);
        rule_func_type_rest(p, node);
    }else{
        add_node(p, parent, RULE_FUNC_TYPE_PARAMS_2);
    }
}

void rule_func_type_rest(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    if(parser_check(p, T_COMMA)){
        tree_node_t *node = add_node(p, parent, RULE_FUNC_TYPE_REST_1);
        if (!node) return;
        parser_expect(p, T_COMMA, node);
        rule_type(p, node);
        rule_func_type_rest(p, node);
    }else{
        add_node(p, parent, RULE_FUNC_TYPE_REST_2);
    }
}

void rule_rhs(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node;
    if(parser_check(p, T_LF)){
        node = add_node(p, parent, RULE_RHS_1);
        if (node) rule_block(p, node);
        return;
    }

    node = add_node(p, parent, RULE_RHS_2);
    if (node) rule_value(p, node);
}

void rule_block(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node = add_node(p, parent, RULE_BLOCK);
    if (!node) return;
    parser_expect(p, T_LF, node);
    parser_expect(p, T_INDENT, node);
    rule_statement_list(p, node);
    parser_expect(p, T_DEDENT, node);
}

void rule_statement_list(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node = add_node(p, parent, RULE_STATEMENT_LIST);
    if (!node) return;
    rule_statement_line(p, node);
    rule_statement_list_rest(p, node);
}

bool first_statement_line_rest(Parser *p){
    if(parser_check(p, KW_VAR) ||
        parser_check(p, KW_VAL) ||
        parser_check(p, KW_RETURN) ||
        parser_check(p, KW_IF) ||
        parser_check(p, KW_WHILE) ||
        parser_check(p, KW_FOR) ||
        parser_check(p, T_ID) ||
        first_expression_noid(p)){

        return true;
    }
    return false;
}

bool first_expression_noid(Parser *p){
    if(parser_check(p, T_INT) ||
        parser_check(p, T_DOUBLE) ||
        parser_check(p, T_STRING) ||
        parser_check(p, KW_TRUE) ||
        parser_check(p, KW_FALSE) ||
        parser_check(p, T_LPARENTH) ||
        parser_check(p, T_UNIT_BRACKETS)){ //TODO interp_begin (s"...), scanner ho zatial nema
            return true;
        }
    return false;
}

void rule_statement_list_rest(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    if(first_statement_line_rest(p)){
        tree_node_t *node = add_node(p, parent, RULE_STATEMENT_LIST_REST_1);
        if (!node) return;
        rule_statement_line(p, node);
        rule_statement_list_rest(p, node);
    }else{
        add_node(p, parent, RULE_STATEMENT_LIST_REST_2);
    }
}

void rule_statement_line(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node = add_node(p, parent, RULE_STATEMENT_LINE);
    if (!node) return;
    rule_statement(p, node);
    rule_statement_line_rest(p, node);
}

void rule_statement_line_rest(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node;
    if(parser_check(p, T_SEMIC)){
        node = add_node(p, parent, RULE_STATEMENT_LINE_REST_1);
        if (!node) return;
        parser_expect(p, T_SEMIC, node);
        rule_statement(p, node);
        rule_statement_line_rest(p, node);
        return;
    }

    node = add_node(p, parent, RULE_STATEMENT_LINE_REST_2);
    if (node) rule_line_end(p, node);
}

void rule_statement(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node;
    if(parser_check(p, KW_VAR) || parser_check(p, KW_VAL)){
        node = add_node(p, parent, RULE_STATEMENT_1);
        if (node) rule_var_definition(p, node);
        return;
    }

    node = add_node(p, parent, RULE_STATEMENT_2);
    if (node) rule_inline_statement(p, node);
}

void rule_inline_statement(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node;
    if(parser_check(p, KW_RETURN)){
        node = add_node(p, parent, RULE_INLINE_STATEMENT_1);
        if (node) rule_return_statement(p, node);
        return;
    }
    if(parser_check(p, KW_IF)){
        node = add_node(p, parent, RULE_INLINE_STATEMENT_2);
        if (node) rule_if_statement(p, node);
        return;
    }
    if(parser_check(p, KW_WHILE)){
        node = add_node(p, parent, RULE_INLINE_STATEMENT_3);
        if (node) rule_while_statement(p, node);
        return;
    }
    if(parser_check(p, KW_FOR)){
        node = add_node(p, parent, RULE_INLINE_STATEMENT_4);
        if (node) rule_for_statement(p, node);
        return;
    }
    if(parser_check(p, T_ID)){
        node = add_node(p, parent, RULE_INLINE_STATEMENT_5);
        if (!node) return;
        parser_expect(p, T_ID, node);
        rule_id_statement(p, node);
        return;
    }

    node = add_node(p, parent, RULE_INLINE_STATEMENT_6);
    if (!node) return;
    rule_expression(p, node);
    rule_match_suffix(p, node);
}

void rule_var_definition(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node = add_node(p, parent, RULE_VAR_DEFINITION);
    if (!node) return;
    rule_var_kind(p, node);
    parser_expect(p, T_ID, node);
    rule_optional_type(p, node);
    parser_expect(p, T_EQU_SIGN, node);
    rule_rhs(p, node);
}

void rule_var_kind(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node;
    if (parser_check(p, KW_VAR)){
        node = add_node(p, parent, RULE_VAR_KIND_2);
        if (node) parser_expect(p, KW_VAR, node);
        return;
    }
    node = add_node(p, parent, RULE_VAR_KIND_1);
    if (node) parser_expect(p, KW_VAL, node);
}

void rule_id_statement(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node;
    if(parser_check(p, T_EQU_SIGN)){
        node = add_node(p, parent, RULE_ID_STATEMENT_1);
        if (!node) return;
        parser_expect(p, T_EQU_SIGN, node);
        rule_rhs(p, node);
        return;
    }
    tree_node_t *id_leaf = tree_get_child(parent, parent->children_count -1);
    tree_detach(id_leaf);
    node = add_node(p, parent, RULE_ID_STATEMENT_2);
    if (!node) {
        tree_dispose(&id_leaf);
        return;
    }
    rule_expression_after_id(p, node, id_leaf);
    rule_match_suffix(p, node);
}


void rule_return_statement(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node = add_node(p, parent, RULE_RETURN_STATEMENT);
    if (!node) return;
    parser_expect(p, KW_RETURN, node);
    rule_return_value(p, node);
}

bool follow_return_value(Parser *p){
    if(parser_check(p, T_LF) ||
        parser_check(p, T_SEMIC) ||
        parser_check(p, KW_ELSE) ||
        parser_check(p, T_COMMA) ||
        parser_check(p, T_RPARENTH)){
            return true;
        }
    return false;
}

void rule_return_value(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    if(!follow_return_value(p)){
        tree_node_t *node = add_node(p, parent, RULE_RETURN_VALUE_1);
        if (node) rule_value(p, node);
    }else{
        add_node(p, parent, RULE_RETURN_VALUE_2);
    }
}

void rule_if_statement(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node = add_node(p, parent, RULE_IF_STATEMENT);
    if (!node) return;
    parser_expect(p, KW_IF, node);
    rule_expression(p, node);
    parser_expect(p, KW_THEN, node);
    rule_body(p, node);
    rule_else_part(p, node);
}

void rule_else_part(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    if(parser_check(p, KW_ELSE)){
        tree_node_t *node = add_node(p, parent, RULE_ELSE_PART_1);
        if (!node) return;
        parser_expect(p, KW_ELSE, node);
        rule_body(p, node);
    }else{
        add_node(p, parent, RULE_ELSE_PART_2);
    }
}

void rule_while_statement(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node = add_node(p, parent, RULE_WHILE_STATEMENT);
    if (!node) return;
    parser_expect(p, KW_WHILE, node);
    rule_expression(p, node);
    parser_expect(p, KW_DO, node);
    rule_body(p, node);
}

void rule_body(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node;
    if(parser_check(p, T_LF)){
        node = add_node(p, parent, RULE_BODY_1);
        if (node) rule_block(p, node);
        return;
    }
    if(parser_check(p, T_MINUS) || parser_check(p, T_NEG)){
        node = add_node(p, parent, RULE_BODY_3);
        if (node) rule_expression(p, node);
        return;
    }

    node = add_node(p, parent, RULE_BODY_2);
    if (node) rule_inline_statement(p, node);
}

void rule_for_statement(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node = add_node(p, parent, RULE_FOR_STATEMENT);
    if (!node) return;
    parser_expect(p, KW_FOR, node);
    rule_for_header(p, node);
    parser_expect(p, KW_DO, node);
    rule_body(p, node);
}

void rule_for_header(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node;
    if(parser_check(p, T_LF)){
        node = add_node(p, parent, RULE_FOR_HEADER_2);
        if (!node) return;
        parser_expect(p, T_LF, node);
        parser_expect(p, T_INDENT, node);
        rule_generator(p, node);
        rule_inline_guards(p, node);
        parser_expect(p, T_LF, node);
        rule_guard_lines(p, node);
        parser_expect(p, T_DEDENT, node);
        return;
    }

    node = add_node(p, parent, RULE_FOR_HEADER_1);
    if (!node) return;
    rule_generator(p, node);
    rule_inline_guards(p, node);
}

void rule_generator(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node = add_node(p, parent, RULE_GENERATOR);
    if (!node) return;
    parser_expect(p, T_ID, node);
    parser_expect(p, T_ASSIG, node);
    rule_expression(p, node);
    rule_range_op(p, node);
    rule_expression(p, node);
    rule_step(p, node);
}

void rule_range_op(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node;
    if(parser_check(p, KW_UNTIL)){
        node = add_node(p, parent, RULE_RANGE_OP_2);
        if (node) parser_expect(p, KW_UNTIL, node);
        return;
    }

    node = add_node(p, parent, RULE_RANGE_OP_1);
    if (node) parser_expect(p, KW_TO, node);
}

void rule_step(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    if(parser_check(p, KW_BY)){
        tree_node_t *node = add_node(p, parent, RULE_STEP_1);
        if (!node) return;
        parser_expect(p, KW_BY, node);
        rule_expression(p, node);
    }else{
        add_node(p, parent, RULE_STEP_2);
    }
}

void rule_inline_guards(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    if(parser_check(p, KW_IF)){
        tree_node_t *node = add_node(p, parent, RULE_INLINE_GUARDS_1);
        if (!node) return;
        parser_expect(p, KW_IF, node);
        rule_expression(p, node);
        rule_inline_guards(p, node);
    }else{
        add_node(p, parent, RULE_INLINE_GUARDS_2);
    }
}

void rule_guard_lines(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    if(parser_check(p, KW_IF)){
        tree_node_t *node = add_node(p, parent, RULE_GUARD_LINES_1);
        if (!node) return;
        parser_expect(p, KW_IF, node);
        rule_expression(p, node);
        rule_inline_guards(p, node);
        parser_expect(p, T_LF, node);
        rule_guard_lines(p, node);
    }else{
        add_node(p, parent, RULE_GUARD_LINES_2);
    }
}

void rule_value(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node;
    if(parser_check(p, KW_IF)){
        node = add_node(p, parent, RULE_VALUE_1);
        if (node) rule_if_statement(p, node);
        return;
    }
    node = add_node(p, parent, RULE_VALUE_2);
    if (!node) return;
    rule_expression(p, node);
    rule_match_suffix(p, node);
}

void rule_match_suffix(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    if(parser_check(p, KW_MATCH)){
        tree_node_t *node = add_node(p, parent, RULE_MATCH_SUFFIX_1);
        if (!node) return;
        parser_expect(p, KW_MATCH, node);
        parser_expect(p, T_LF, node);
        parser_expect(p, T_INDENT, node);
        rule_case_list(p, node);
        parser_expect(p, T_DEDENT, node);
    }else{
        add_node(p, parent, RULE_MATCH_SUFFIX_2);
    }
}

void rule_case_list(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node = add_node(p, parent, RULE_CASE_LIST);
    if (!node) return;
    rule_case_clause(p, node);
    rule_case_list_rest(p, node);
}

void rule_case_list_rest(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    if(parser_check(p, KW_CASE)){
        tree_node_t *node = add_node(p, parent, RULE_CASE_LIST_REST_1);
        if (!node) return;
        rule_case_clause(p, node);
        rule_case_list_rest(p, node);
    }else{
        add_node(p, parent, RULE_CASE_LIST_REST_2);
    }
}

void rule_case_clause(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node = add_node(p, parent, RULE_CASE_CLAUSE);
    if (!node) return;
    parser_expect(p, KW_CASE, node);
    rule_pattern(p, node);
    rule_case_guard(p, node);
    parser_expect(p, T_LAMB, node);
    rule_rhs(p, node);
    rule_line_end(p, node);
}

void rule_case_guard(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    if(parser_check(p, KW_IF)){
        tree_node_t *node = add_node(p, parent, RULE_CASE_GUARD_1);
        if (!node) return;
        parser_expect(p, KW_IF, node);
        rule_expression(p, node);
    }else{
        add_node(p, parent, RULE_CASE_GUARD_2);
    }
}

void rule_pattern(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node;
    if(parser_check(p, T_REPRESPATTERN)){
        node = add_node(p, parent, RULE_PATTERN_1);
        if (node) parser_expect(p, T_REPRESPATTERN, node);
        return;
    }
    if(parser_check(p, T_ID)){
        node = add_node(p, parent, RULE_PATTERN_2);
        if (!node) return;
        parser_expect(p, T_ID, node);
        rule_id_pattern(p, node);
        return;
    }

    node = add_node(p, parent, RULE_PATTERN_3);
    if (node) rule_literal_pattern(p, node);
}

void rule_id_pattern(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node;
    if(parser_check(p, T_COL)){
        node = add_node(p, parent, RULE_ID_PATTERN_1);
        if (!node) return;
        parser_expect(p, T_COL, node);
        rule_type(p, node);
        return;
    }

    node = add_node(p, parent, RULE_ID_PATTERN_2);
    if (!node) return;
    if(parser_check(p, T_UNIT_BRACKETS)){
        parser_expect(p, T_UNIT_BRACKETS, node); // '(' ')' chodi zo scannera ako jeden token
        return;
    }
    parser_expect(p, T_LPARENTH, node);
    rule_subpattern_list(p, node);
    parser_expect(p, T_RPARENTH, node);
}

bool first_literal_pattern(Parser *p){
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

bool first_subpattern(Parser *p){
    if(parser_check(p, T_ID) ||
        parser_check(p, T_REPRESPATTERN) ||
        first_literal_pattern(p)){
            return true;
        }
    return false;
}

void rule_subpattern_list(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    if(first_subpattern(p)){
        tree_node_t *node = add_node(p, parent, RULE_SUBPATTERN_LIST_1);
        if (!node) return;
        rule_subpattern(p, node);
        rule_subpattern_rest(p, node);
    }else{
        add_node(p, parent, RULE_SUBPATTERN_LIST_2);
    }
}

void rule_subpattern_rest(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    if(parser_check(p, T_COMMA)){
        tree_node_t *node = add_node(p, parent, RULE_SUBPATTERN_REST_1);
        if (!node) return;
        parser_expect(p, T_COMMA, node);
        rule_subpattern(p, node);
        rule_subpattern_rest(p, node);
    }else{
        add_node(p, parent, RULE_SUBPATTERN_REST_2);
    }
}

void rule_subpattern(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node;
    if(parser_check(p, T_ID)){
        node = add_node(p, parent, RULE_SUBPATTERN_1);
        if (node) parser_expect(p, T_ID, node);
        return;
    }
    if(parser_check(p, T_REPRESPATTERN)){
        node = add_node(p, parent, RULE_SUBPATTERN_2);
        if (node) parser_expect(p, T_REPRESPATTERN, node);
        return;
    }

    node = add_node(p, parent, RULE_SUBPATTERN_3);
    if (node) rule_literal_pattern(p, node);
}

void rule_literal_pattern(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node;
    if(parser_check(p, T_INT)){
        node = add_node(p, parent, RULE_LITERAL_PATTERN_1);
        if (node) parser_expect(p, T_INT, node);
        return;
    }
    if(parser_check(p, T_DOUBLE)){
        node = add_node(p, parent, RULE_LITERAL_PATTERN_2);
        if (node) parser_expect(p, T_DOUBLE, node);
        return;
    }
    if(parser_check(p, T_STRING)){
        node = add_node(p, parent, RULE_LITERAL_PATTERN_3);
        if (node) parser_expect(p, T_STRING, node);
        return;
    }
    if(parser_check(p, KW_TRUE)){
        node = add_node(p, parent, RULE_LITERAL_PATTERN_4);
        if (node) parser_expect(p, KW_TRUE, node);
        return;
    }
    if(parser_check(p, KW_FALSE)){
        node = add_node(p, parent, RULE_LITERAL_PATTERN_4);
        if (node) parser_expect(p, KW_FALSE, node);
        return;
    }
    if(parser_check(p, T_MINUS)){
        node = add_node(p, parent, RULE_LITERAL_PATTERN_5);
        if (!node) return;
        parser_expect(p, T_MINUS, node);
        rule_number(p, node);
        return;
    }

    node = add_node(p, parent, RULE_LITERAL_PATTERN_6);
    if (node) parser_expect(p, T_UNIT_BRACKETS, node); // '(' ')' chodi zo scannera ako jeden token
}

void rule_number(Parser *p, tree_node_t *parent){
    if (p->error != PARSE_OK) return;
    tree_node_t *node;
    if(parser_check(p, T_INT)){
        node = add_node(p, parent, RULE_NUMBER_1);
        if (node) parser_expect(p, T_INT, node);
        return;
    }

    node = add_node(p, parent, RULE_NUMBER_2);
    if (node) parser_expect(p, T_DOUBLE, node);
}


// Reads the next token that is not whitespace into p->current
bool fetch_token(Parser *p) {
    // type of the previous token 
    bool after_lf = (p->current.type == T_LF);
    bool skip;
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
        #ifdef DEBUG_TOKENS
            fprintf(stderr, "Token: type = %d, line = %d and data = %s\n",
                    p->current.type, p->current.line,
                    p->current.data ? p->current.data : "NULL");
        #endif
        // whitespace is always skipped, mutliple contiguous T_LF is skipped also
        skip = p->current.type == T_SPACE ||
               (after_lf && p->current.type == T_LF);

        // inside parentheses a line break is only whitespace 
        if (!skip && p->paren_depth > 0) {
            t_type type = p->current.type;
            if (type == T_LF || type == T_INDENT || type == T_DEDENT) {
                if (type == T_LF) p->paren_newline = true;
                skip = true;
            } else if (p->paren_newline && type != T_EOF) {
                // first token of a continuation line: all INDENT/DEDENT of this
                // line were already processed
                p->paren_newline = false;
                int indent = p->pole_dent[p->pos];
                bool ok = indent > p->paren_base_indent ||
                          (type == T_RPARENTH && indent == p->paren_base_indent);
                if (!ok) {
                    free(p->current.data);
                    p->current.data = NULL;
                    if (p->error == PARSE_OK) {
                        p->error = PARSE_ERR_LEX;
                    }
                    return false;
                }
            }
        }

        if (skip) {
            free(p->current.data);
            p->current.data = NULL;
        }
    } while (skip);

    if (p->current.type == T_LPARENTH) {
        if (p->paren_depth == 0) {
            p->paren_base_pos = p->pos;
            p->paren_base_indent = p->pole_dent[p->pos];
        }
        p->paren_depth++;
    } else if (p->current.type == T_RPARENTH && p->paren_depth > 0) {
        p->paren_depth--;
        if (p->paren_depth == 0) {
            // drop indentation levels pushed by continuation lines, so the
            // scanner continues as if the parentheses were on one line
            p->pos = p->paren_base_pos;
            p->mul_de = (p->pole_dent[p->pos] != 0);
            p->paren_newline = false;
        }
    }



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
    p->paren_depth = 0;
    p->paren_base_pos = 0;
    p->paren_base_indent = 0;
    p->paren_newline = false;
    p->prev_type = T_EOF; // nothing consumed yet, any type other than T_DEDENT
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
    p->prev_type = p->current.type;
    free(p->current.data);
    p->current.data = NULL;
    return fetch_token(p);
}

// Consumes the expected token and attaches it as a leaf to parent
bool parser_expect(Parser *p, t_type type, tree_node_t *parent) {
    if (p->error != PARSE_OK) {
        return false;
    }
    if (p->current.type != type) {
        p->error = PARSE_ERR_SYNTAX;
        return false;
    }

    // leaf must be created before advance, which frees current.data
    tree_node_t *leaf = tree_create_terminal(&p->current);
    if (!leaf || !tree_add_child(parent, leaf)) {
        tree_dispose(&leaf);
        p->error = PARSE_ERR_INTERNAL;
        return false;
    }

    return parser_advance(p);
}
