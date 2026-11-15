# include "generator.h"

#include <stdarg.h>
#include <stdlib.h>

// array of tree nodes
typedef struct {
    const tree_node_t **items;
    size_t count;
    size_t capacity;
} node_list_t;

typedef struct {
    FILE *out;
    const SemanticInfo *info;
    node_list_t decls;
    int error;
} gen_t;

// writes one line of code
static void emit(gen_t *g, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(g->out, fmt, args);
    fputc('\n', g->out);
}

// appends a node to the list (sets error 99 on failure)
static void list_push(gen_t *g, node_list_t *l, const tree_node_t *node) {
    if (l->count == l->capacity) {
        size_t cap = l->capacity ? l->capacity * 2 : 16;
        const tree_node_t **tmp = realloc(l->items, cap * sizeof(*tmp));
        if (!tmp) {
            g->error = 99;
            return;
        }
        l->items = tmp;
        l->capacity = cap;
    }
    l->items[l->count++] = node;
}

// unique number of a declaration
static size_t decl_id(gen_t *g, const tree_node_t *decl) {
    for (size_t i = 0; i < g->decls.count; i++) {
        if (g->decls.items[i] == decl) {
            return i;
        }
    }
    list_push(g, &g->decls, decl);
    return g->decls.count - 1;
}

// name in the source: VAR_DEFINITION has it as child 1, PARAM as child 0
static const char *decl_name(const tree_node_t *decl) {
    int idx = (decl->rule == RULE_PARAM) ? 0 : 1;
    return decl->children[idx]->token.data;
}

// finds all nodes with the given rule in the subtree
static void collect(gen_t *g, const tree_node_t *node, tree_rule_t rule, node_list_t *out) {
    if (!node || node->type != NODE_NONTERMINAL || node->rule == RULE_VALUE_EXPRESSION) {
        return;
    }
    if (node->rule == rule) {
        list_push(g, out, node);
    }
    for (size_t i = 0; i < node->children_count; i++) {
        collect(g, node->children[i], rule, out);
    }
}

// returns the FUNCTION_DEF of a top-level item, or NULL if it is not a function
static const tree_node_t *item_function(const tree_node_t *item) {
    switch (item->rule) {
        case RULE_TOP_ITEM_1:
            return item->children[2];
        case RULE_TOP_ITEM_2:
            return item->children[0];
        default:
            return NULL; 
    }
}

static const char *function_name(const tree_node_t *def) {
    return def->children[1]->token.data;
}

// generates one function: label, its own frame, body, return
static void gen_function(gen_t *g, const tree_node_t *def) {
    emit(g, "# --- function %s ---", function_name(def));
    emit(g, "LABEL $f_%s", function_name(def));
    emit(g, "PUSHFRAME");
    
    node_list_t params = {0};
    collect(g, def->children[2], RULE_PARAM, &params);
    for (size_t i = 0; i < params.count; i++) {
        const tree_node_t *p = params.items[i];
        emit(g, "DEFVAR LF@%s$%zu", decl_name(p), decl_id(g, p));
    }
    for (size_t i = params.count; i > 0; i--) {
        const tree_node_t *p = params.items[i - 1];
        emit(g, "POPS LF@%s$%zu", decl_name(p), decl_id(g, p));
    }
    free(params.items);

    // all local variables defined once, at the start of the function
    node_list_t locals = {0};
    collect(g, def->children[5], RULE_VAR_DEFINITION, &locals);
    for (size_t i = 0; i < locals.count; i++) {
        const tree_node_t *v = locals.items[i];
        emit(g, "DEFVAR LF@%s$%zu", decl_name(v), decl_id(g, v));
    }
    free(locals.items);

    emit(g, "PUSHS nil@nil");
    emit(g, "POPFRAME");
    emit(g, "RETURN");
}

int generate_code(const tree_node_t *root, const SemanticInfo *info, FILE *out) {
    gen_t g = {out, info, {0}, 0};

    const tree_node_t *top = tree_get_child(root, 2);

    //find the function marked with @main
    const tree_node_t *main_def = NULL;
    for (const tree_node_t *list = top; list && list->rule == RULE_TOP_LIST_1; list = list->children[1]) {
        if (list->children[0]->rule == RULE_TOP_ITEM_1) {
            main_def = item_function(list->children[0]);
        }
    }
    if (!main_def) {
        return 99;
    }

    // program start: call main, end with exit code 0
    emit(&g, ".IFJcode26");
    emit(&g, "CREATEFRAME");
    emit(&g, "CALL $f_%s", function_name(main_def));
    emit(&g, "CLEARS");
    emit(&g, "EXIT int@0");

    // all functions
    for (const tree_node_t *list = top; list && list->rule == RULE_TOP_LIST_1; list = list->children[1]) {
        const tree_node_t *def = item_function(list->children[0]);
        if (def) {
            gen_function(&g, def);
        }
    }
    free(g.decls.items);
    return g.error;
}