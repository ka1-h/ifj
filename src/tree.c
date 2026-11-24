// author: Daniel Čerešňa <xceresd00>

#include "tree.h"

#include <stdlib.h>
#include <string.h>

static char* copy_string(const char* s)
{
    size_t len = strlen(s) + 1;
    char* copy = malloc(len);
    if (copy) {
        memcpy(copy, s, len);
    }
    return copy;
}

static tree_node_t* tree_new(tree_node_type_t type)
{
    tree_node_t* node = calloc(1, sizeof(tree_node_t));
    if (!node) {
        return NULL;
    }
    node->type = type;
    node->rule = TREE_RULE_NONE;
    return node;
}

tree_node_t* tree_create_terminal(const Token* token)
{
    if (!token) {
        return NULL;
    }

    tree_node_t* node = tree_new(NODE_TERMINAL);
    if (!node) {
        return NULL;
    }

    node->token.data = copy_string(token->data ? token->data : "");
    if (!node->token.data) {
        free(node);
        return NULL;
    }
    node->token.type = token->type;
    node->token.line = token->line;
    return node;
}

tree_node_t* tree_create_nonterminal(tree_rule_t rule)
{
    tree_node_t* node = tree_new(NODE_NONTERMINAL);
    if (node) {
        node->rule = rule;
    }
    return node;
}

bool tree_add_child(tree_node_t* parent, tree_node_t* child)
{
    if (!parent || !child || child->parent) {
        return false;
    }

    if (parent->children_count == parent->children_capacity) {
        size_t new_capacity = parent->children_capacity ? parent->children_capacity * 2 : 4;
        tree_node_t** tmp = realloc(parent->children, new_capacity * sizeof(tree_node_t*));
        if (!tmp) {
            return false;
        }
        parent->children = tmp;
        parent->children_capacity = new_capacity;
    }

    parent->children[parent->children_count++] = child;
    child->parent = parent;
    return true;
}

bool tree_detach(tree_node_t* node)
{
    if (!node || !node->parent) {
        return false;
    }

    tree_node_t* parent = node->parent;
    for (size_t i = 0; i < parent->children_count; i++) {
        if (parent->children[i] == node) {
            memmove(&parent->children[i], &parent->children[i + 1],
                (parent->children_count - i - 1) * sizeof(tree_node_t*));
            parent->children_count--;
            node->parent = NULL;
            return true;
        }
    }
    return false;
}

void tree_remove(tree_node_t* node)
{
    if (!node) {
        return;
    }
    tree_detach(node);
    tree_dispose(&node);
}

void tree_dispose(tree_node_t** root)
{
    if (!root || !*root) {
        return;
    }

    tree_node_t* node = *root;
    for (size_t i = 0; i < node->children_count; i++) {
        tree_node_t* child = node->children[i];
        child->parent = NULL; // avoid detach work in the recursion
        tree_dispose(&child);
    }
    free(node->children);
    if (node->type == NODE_TERMINAL) {
        free(node->token.data);
    }
    free(node);
    *root = NULL;
}

tree_node_t* tree_get_child(const tree_node_t* node, size_t index)
{
    if (!node || index >= node->children_count) {
        return NULL;
    }
    return node->children[index];
}

const char* tree_rule_name(tree_rule_t rule)
{
    static const char* const rule_names[] = {
        "PROGRAM",
        "PROLOG",
        "TOP_LIST_1",
        "TOP_LIST_2",
        "TOP_ITEM_1",
        "TOP_ITEM_2",
        "TOP_ITEM_3",
        "TOP_ITEM_4",
        "TOP_ITEM_5",
        "ANNOTATION_EOL_1",
        "ANNOTATION_EOL_2",
        "FUNCTION_DEF",
        "PARAM_LISTS_1",
        "PARAM_LISTS_2",
        "PARAM_LIST_1",
        "PARAM_LIST_2",
        "PARAM_LIST_REST_1",
        "PARAM_LIST_REST_2",
        "PARAM_AFTER_COMMA_1",
        "PARAM_AFTER_COMMA_2",
        "PARAM",
        "OPTIONAL_TYPE_1",
        "OPTIONAL_TYPE_2",
        "TYPE_1",
        "TYPE_2",
        "UNION_REST_1",
        "UNION_REST_2",
        "SIMPLE_TYPE_1",
        "SIMPLE_TYPE_2",
        "SIMPLE_TYPE_3",
        "SIMPLE_TYPE_4",
        "SIMPLE_TYPE_5",
        "SIMPLE_TYPE_6",
        "FUNC_TYPE_PARAMS_1",
        "FUNC_TYPE_PARAMS_2",
        "FUNC_TYPE_REST_1",
        "FUNC_TYPE_REST_2",
        "TYPE_DEF",
        "CASE_CLASS_DEF",
        "EXTENSION_DEF",
        "METHOD_LIST",
        "METHOD_LIST_REST_1",
        "METHOD_LIST_REST_2",
        "RHS_1",
        "RHS_2",
        "BLOCK",
        "STATEMENT_LIST",
        "STATEMENT_LIST_REST_1",
        "STATEMENT_LIST_REST_2",
        "STATEMENT_LINE",
        "STATEMENT_LINE_REST_1",
        "STATEMENT_LINE_REST_2",
        "STATEMENT_1",
        "STATEMENT_2",
        "INLINE_STATEMENT_1",
        "INLINE_STATEMENT_2",
        "INLINE_STATEMENT_3",
        "INLINE_STATEMENT_4",
        "INLINE_STATEMENT_5",
        "INLINE_STATEMENT_6",
        "VAR_DEFINITION",
        "VAR_KIND_1",
        "VAR_KIND_2",
        "ID_STATEMENT_1",
        "ID_STATEMENT_2",
        "RETURN_STATEMENT",
        "RETURN_VALUE_1",
        "RETURN_VALUE_2",
        "IF_STATEMENT",
        "ELSE_PART_1",
        "ELSE_PART_2",
        "WHILE_STATEMENT",
        "BODY_1",
        "BODY_2",
        "BODY_3",
        "FOR_STATEMENT",
        "FOR_HEADER_1",
        "FOR_HEADER_2",
        "GENERATOR",
        "RANGE_OP_1",
        "RANGE_OP_2",
        "STEP_1",
        "STEP_2",
        "INLINE_GUARDS_1",
        "INLINE_GUARDS_2",
        "GUARD_LINES_1",
        "GUARD_LINES_2",
        "VALUE_1",
        "VALUE_2",
        "MATCH_SUFFIX_1",
        "MATCH_SUFFIX_2",
        "CASE_LIST",
        "CASE_LIST_REST_1",
        "CASE_LIST_REST_2",
        "CASE_CLAUSE",
        "CASE_GUARD_1",
        "CASE_GUARD_2",
        "PATTERN_1",
        "PATTERN_2",
        "PATTERN_3",
        "ID_PATTERN_1",
        "ID_PATTERN_2",
        "SUBPATTERN_LIST_1",
        "SUBPATTERN_LIST_2",
        "SUBPATTERN_REST_1",
        "SUBPATTERN_REST_2",
        "SUBPATTERN_1",
        "SUBPATTERN_2",
        "SUBPATTERN_3",
        "LITERAL_PATTERN_1",
        "LITERAL_PATTERN_2",
        "LITERAL_PATTERN_3",
        "LITERAL_PATTERN_4",
        "LITERAL_PATTERN_5",
        "LITERAL_PATTERN_6",
        "NUMBER_1",
        "NUMBER_2",
        "CALL_ARGS",
        "ARG_LIST_1",
        "ARG_LIST_2",
        "ARG_LIST_REST_1",
        "ARG_LIST_REST_2",
        "ARG_AFTER_COMMA_1",
        "ARG_AFTER_COMMA_2",
        "LAMBDA_AFTER_ID",
        "LAMBDA_ARROW",
        "INTERP_STRING",
        "INTERP_PARTS_1",
        "INTERP_PARTS_2",
        "INTERP_PARTS_3",
        "INTERP_PARTS_4",
        "EXPRESSION",
        "VALUE_EXPRESSION",
        "EXPRESSION_AFTER_ID",
        "EXPRESSION_NOID",
        "EXPRESSION_UNARY",
        "LEADING_EOL_1",
        "LEADING_EOL_2",
    };
    if (rule < 0 || rule >= RULE_COUNT) {
        return "NONE";
    }
    return rule_names[rule];
}

static void print_label(const tree_node_t* node, FILE* out)
{
    if (node->type == NODE_NONTERMINAL) {
        fprintf(out, "<%s>", tree_rule_name(node->rule));
    } else if (node->token.type == T_LF) {
        fputs("\\n", out);
    } else {
        fprintf(out, "%s", node->token.data);
    }
}

static void print_rec(const tree_node_t* node, FILE* out, const char* prefix, bool is_last)
{
    fputs(prefix, out);
    fputs(is_last ? "└── " : "├── ", out);
    print_label(node, out);
    fputc('\n', out);

    static const char* const pad_last = "    ";
    static const char* const pad_more = "│   ";
    const char* pad = is_last ? pad_last : pad_more;

    char* child_prefix = malloc(strlen(prefix) + strlen(pad) + 1);
    if (!child_prefix) {
        return;
    }
    strcpy(child_prefix, prefix);
    strcat(child_prefix, pad);

    for (size_t i = 0; i < node->children_count; i++) {
        print_rec(node->children[i], out, child_prefix, i + 1 == node->children_count);
    }
    free(child_prefix);
}

void tree_print(const tree_node_t* root, FILE* out)
{
    if (!root || !out) {
        return;
    }
    print_label(root, out);
    fputc('\n', out);

    for (size_t i = 0; i < root->children_count; i++) {
        print_rec(root->children[i], out, "", i + 1 == root->children_count);
    }
}
