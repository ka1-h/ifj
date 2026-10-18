#ifndef TREE_H
#define TREE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "scanner.h"
#include "symtable.h"

typedef enum {
    TREE_RULE_NONE = -1,
    RULE_PROGRAM,
    RULE_PROLOG,
    RULE_TOP_LIST_1,
    RULE_TOP_LIST_2,
    RULE_TOP_ITEM_1,
    RULE_TOP_ITEM_2,
    RULE_TOP_ITEM_3,
    RULE_TOP_ITEM_4,
    RULE_TOP_ITEM_5,
    RULE_ANNOTATION_EOL_1,
    RULE_ANNOTATION_EOL_2,
    RULE_FUNCTION_DEF,
    RULE_PARAM_LISTS_1,
    RULE_PARAM_LISTS_2,
    RULE_PARAM_LIST_1,
    RULE_PARAM_LIST_2,
    RULE_PARAM_LIST_REST_1,
    RULE_PARAM_LIST_REST_2,
    RULE_PARAM_AFTER_COMMA_1,
    RULE_PARAM_AFTER_COMMA_2,
    RULE_PARAM,
    RULE_OPTIONAL_TYPE_1,
    RULE_OPTIONAL_TYPE_2,
    RULE_TYPE_1,
    RULE_TYPE_2,
    RULE_UNION_REST_1,
    RULE_UNION_REST_2,
    RULE_SIMPLE_TYPE_1,
    RULE_SIMPLE_TYPE_2,
    RULE_SIMPLE_TYPE_3,
    RULE_SIMPLE_TYPE_4,
    RULE_SIMPLE_TYPE_5,
    RULE_SIMPLE_TYPE_6,
    RULE_FUNC_TYPE_PARAMS_1,
    RULE_FUNC_TYPE_PARAMS_2,
    RULE_FUNC_TYPE_REST_1,
    RULE_FUNC_TYPE_REST_2,
    RULE_TYPE_DEF,
    RULE_CASE_CLASS_DEF,
    RULE_EXTENSION_DEF,
    RULE_METHOD_LIST,
    RULE_METHOD_LIST_REST_1,
    RULE_METHOD_LIST_REST_2,
    RULE_RHS_1,
    RULE_RHS_2,
    RULE_BLOCK,
    RULE_STATEMENT_LIST,
    RULE_STATEMENT_LIST_REST_1,
    RULE_STATEMENT_LIST_REST_2,
    RULE_STATEMENT_LINE,
    RULE_STATEMENT_LINE_REST_1,
    RULE_STATEMENT_LINE_REST_2,
    RULE_STATEMENT_1,
    RULE_STATEMENT_2,
    RULE_INLINE_STATEMENT_1,
    RULE_INLINE_STATEMENT_2,
    RULE_INLINE_STATEMENT_3,
    RULE_INLINE_STATEMENT_4,
    RULE_INLINE_STATEMENT_5,
    RULE_INLINE_STATEMENT_6,
    RULE_VAR_DEFINITION,
    RULE_VAR_KIND_1,
    RULE_VAR_KIND_2,
    RULE_ID_STATEMENT_1,
    RULE_ID_STATEMENT_2,
    RULE_RETURN_STATEMENT,
    RULE_RETURN_VALUE_1,
    RULE_RETURN_VALUE_2,
    RULE_IF_STATEMENT,
    RULE_ELSE_PART_1,
    RULE_ELSE_PART_2,
    RULE_WHILE_STATEMENT,
    RULE_BODY_1,
    RULE_BODY_2,
    RULE_BODY_3,
    RULE_FOR_STATEMENT,
    RULE_FOR_HEADER_1,
    RULE_FOR_HEADER_2,
    RULE_GENERATOR,
    RULE_RANGE_OP_1,
    RULE_RANGE_OP_2,
    RULE_STEP_1,
    RULE_STEP_2,
    RULE_INLINE_GUARDS_1,
    RULE_INLINE_GUARDS_2,
    RULE_GUARD_LINES_1,
    RULE_GUARD_LINES_2,
    RULE_VALUE_1,
    RULE_VALUE_2,
    RULE_MATCH_SUFFIX_1,
    RULE_MATCH_SUFFIX_2,
    RULE_CASE_LIST,
    RULE_CASE_LIST_REST_1,
    RULE_CASE_LIST_REST_2,
    RULE_CASE_CLAUSE,
    RULE_CASE_GUARD_1,
    RULE_CASE_GUARD_2,
    RULE_PATTERN_1,
    RULE_PATTERN_2,
    RULE_PATTERN_3,
    RULE_ID_PATTERN_1,
    RULE_ID_PATTERN_2,
    RULE_SUBPATTERN_LIST_1,
    RULE_SUBPATTERN_LIST_2,
    RULE_SUBPATTERN_REST_1,
    RULE_SUBPATTERN_REST_2,
    RULE_SUBPATTERN_1,
    RULE_SUBPATTERN_2,
    RULE_SUBPATTERN_3,
    RULE_LITERAL_PATTERN_1,
    RULE_LITERAL_PATTERN_2,
    RULE_LITERAL_PATTERN_3,
    RULE_LITERAL_PATTERN_4,
    RULE_LITERAL_PATTERN_5,
    RULE_LITERAL_PATTERN_6,
    RULE_NUMBER_1,
    RULE_NUMBER_2,
    RULE_CALL_ARGS,
    RULE_ARG_LIST_1,
    RULE_ARG_LIST_2,
    RULE_ARG_LIST_REST_1,
    RULE_ARG_LIST_REST_2,
    RULE_ARG_AFTER_COMMA_1,
    RULE_ARG_AFTER_COMMA_2,
    RULE_LAMBDA_AFTER_ID,
    RULE_LAMBDA_ARROW,
    RULE_INTERP_STRING,
    RULE_INTERP_PARTS_1,
    RULE_INTERP_PARTS_2,
    RULE_INTERP_PARTS_3,
    RULE_INTERP_PARTS_4,
    // nonterminals expanded by the precedence parser 
    RULE_EXPRESSION,
    RULE_VALUE_EXPRESSION,
    RULE_EXPRESSION_AFTER_ID,
    RULE_EXPRESSION_NOID,
    RULE_EXPRESSION_UNARY,
    // helper nonterminals
    RULE_LEADING_EOL_1,
    RULE_LEADING_EOL_2,
    RULE_COUNT
} tree_rule_t;

const char *tree_rule_name(tree_rule_t rule);

typedef enum {
    NODE_TERMINAL,
    NODE_NONTERMINAL
} tree_node_type_t;


typedef struct {
    t_type type;
    char *data;    
    int line;
} tree_token_t;

typedef struct tree_node {
    tree_node_type_t type;
    struct tree_node *parent;       
    struct tree_node **children;    
    size_t children_count;
    size_t children_capacity;

    tree_rule_t rule;              
    tree_token_t token;           
    void *symbol;                  
} tree_node_t;

tree_node_t *tree_create_terminal(const Token *token);
tree_node_t *tree_create_nonterminal(tree_rule_t rule);
bool tree_add_child(tree_node_t *parent, tree_node_t *child);
bool tree_detach(tree_node_t *node);
void tree_remove(tree_node_t *node);
void tree_dispose(tree_node_t **root);
tree_node_t *tree_get_child(const tree_node_t *node, size_t index);
void tree_print(const tree_node_t *root, FILE *out);

#endif
