# include "generator.h"

#include <stdarg.h>

typedef struct {
    FILE *out;
    const SemanticInfo *info;
} gen_t;

// writes one line of code
static void emit(gen_t *g, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(g->out, fmt, args);
    fputc('\n', g->out);
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
    // TODO parameters and body
    emit(g, "PUSHS nil@nil");
    emit(g, "POPFRAME");
    emit(g, "RETURN");
}

int generate_code(const tree_node_t *root, const SemanticInfo *info, FILE *out) {
    gen_t g = {out, info};

    const tree_node_t *top = tree_get_child(root, 2);

    //find the function marked with @main
    const tree_node_t *main_def = NULL;
    for (const tree_node_t * list = top; list && list->rule == RULE_TOP_LIST_1; list = list->children[1]) {
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
    return 0;
}