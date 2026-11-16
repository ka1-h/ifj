# include "generator.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

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
    va_end(args);
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

// type of an expression node, Double if semantics marked it for conversion
static DataType expr_type(gen_t *g, const tree_node_t *node) {
    const SemanticNodeInfo *i = semantic_info_find(g->info, node);
    if (!i) {
        return TYPE_UNDEF;
    }
    return i->promote_to_double ? TYPE_DOUBLE : i->type;
}

// writes "<instr> LF@name$id" for the variable an identifier leaf refers to
static void emit_var(gen_t *g, const char *instr, const tree_node_t *id_leaf) {
    const SemanticNodeInfo *i = semantic_info_find(g->info, id_leaf);
    if (!i || !i->declaration) {
        g->error = 99;
        return;
    }
    emit(g, "%s LF@%s$%zu", instr, decl_name(i->declaration), decl_id(g, i->declaration));
}

// LITERALS
// 1_000 -> int@1000
static void emit_int(gen_t *g, const char *text) {
    fputs("PUSHS int@", g->out);
    for (const char *c = text; *c; c++) {
        if (*c != '_') {
            fputc(*c, g->out);
        }
    }
    fputc('\n', g->out);
}

// writes a double literal
static void emit_double(gen_t *g, const char *text) {
    char buf[256];
    size_t n = 0;
    for (const char *c = text; *c && n < sizeof(buf) - 1; c++) {
        if (*c != '_' && *c != 'd' && *c != 'D') {
            buf[n++] = *c;
        }
    }
    buf[n] = '\0';
    emit(g, "PUSHS float@%a", strtod(buf, NULL));
}

// one character of a string constant
static void put_char(gen_t *g, unsigned char c) {
    if (c <= 32 || c == '#' || c == '\\') {
        fprintf(g->out, "\\%03d", c);
    } else {
        fputc(c, g->out);
    }
}

// "..." with escape sequences, or """..."""
static void emit_string(gen_t *g, const char *text) {
    size_t len = strlen(text);
    fputs("PUSHS string@", g->out);
    if (len >= 6 && strncmp(text, "\"\"\"", 3) == 0) {
        for (size_t i = 3; i < len - 3; i++) {
            put_char(g, (unsigned char)text[i]);
        }
    } else {
        for (size_t i = 1; i + 1 < len; i++) {
            char c = text[i];
            if (c == '\\' && i + 2 < len) {
                i++;
                switch (text[i])
                {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                default: c = text[i]; break;
                }
            }
            put_char(g, (unsigned char)c);
        }
    }
    fputc('\n', g->out);
}

// EXRESSIONS
static void gen_expr(gen_t *g, const tree_node_t *node);
static void gen_value(gen_t *g, const tree_node_t *value);

// leaf: variable or literal
static void gen_term(gen_t *g, const tree_node_t *leaf) {
    switch (leaf->token.type) {
        case T_ID: emit_var(g, "PUSHS", leaf); break;
        case T_INT: emit_int(g, leaf->token.data); break;
        case T_DOUBLE: emit_double(g, leaf->token.data); break;
        case T_STRING: emit_string(g, leaf->token.data); break;
        case T_UNIT_BRACKETS: emit(g, "PUSHS nil@nil"); break;
        case KW_TRUE: emit(g, "PUSHS bol@true"); break;
        case KW_FALSE: emit(g, "PUSHS bool@false"); break;
        default: g->error = 99; break;
    }
}

// n-th argument (VALUE node) of a CALL_ARGS node, or NULL
static const tree_node_t *call_arg(const tree_node_t *args, size_t n) {
    for (size_t i = 0; i < args->children_count; i++) {
        if (args->children[i]->type == NODE_NONTERMINAL && n-- == 0) {
            return args->children[i];
        }
    }
    return NULL;
}

//function call: every call leaves exactly one value on the stack
static void gen_call(gen_t *g, const tree_node_t *call) {
    const char *name = call->children[0]->token.data;
    const tree_node_t *args = call->children[1];

    if (strcmp(name, "print") == 0) {
        const tree_node_t *arg = call_arg(args, 0);
        gen_value(g, arg);
        emit(g, "POPS GF@$t1");
        if (expr_type(g, arg->children[0]) == TYPE_UNIT) {
            emit(g, "WRITE string@()");
        } else {
            emit(g, "WRITE GF@$t1");
        }
        emit(g, "PUSHS nil@nil");
        return;
    }
    emit(g, "# TODO call %s", name);
    emit(g, "PUSHS nil@nil");
}

// -x as 0 - x, !x as NOT x
static void gen_unary(gen_t *g, const tree_node_t *node) {
    const tree_node_t *operand = node->children[1];
    if (node->children[0]->token.type == T_NEG) {
        gen_expr(g, operand);
        emit(g, "NOTS");
        return;
    }
    if (expr_type(g, operand) == TYPE_DOUBLE) {
        emit(g, "PUSHS float@0x0p+0");
    } else {
        emit(g, "PUSHS int@0");
    }
    gen_expr(g, operand);
    emit(g, "SUBS");
}

// E op E: both operands on the stack, then one stack instruction
static void gen_binary(gen_t *g, const tree_node_t *node) {
    const tree_node_t *left = node->children[0];
    const tree_node_t *right = node->children[2];
    t_type op = node->children[1]->token.type;

    if (op == T_DOT || op == T_AND || op == T_OR) {
        emit(g, "# TODO operator %s", node->children[1]->token.data);
        emit(g, "PUSHS nil@nil");
        return;
    }

    gen_expr(g, left);
    gen_expr(g, right);
    DataType t = expr_type(g, left);

    switch (op) {
        case T_PLUS:
            if (t == TYPE_STRING) {
                emit(g, "POPS GF@$t2");
                emit(g, "POPS GF@$t1");
                emit(g, "CONCAT GF@$t3 GF@$t1 GF@$t2");
                emit(g, "PUSHS GF@$t3");
            } else {
                emit(g, "ADDS");
            }
            break;
        case T_MINUS:
            emit(g, "SUBS");
            break;
        case T_MULT:
            emit(g, "MULS");
            break;
        case T_DIV:
            emit(g, t == TYPE_DOUBLE ? "DIVS" : "IDIVS");
            break;
        case T_LT:
            emit(g, "LTS");
            break;
        case T_GT:
            emit(g, "GTS");
            break;
        case T_ET:
            emit(g, "EQS");
            break;
        case T_NET:
            emit(g, "EQS");
            emit(g, "NOTS");
            break;
        case T_LOET:
            emit(g, "GTS");
            emit(g, "NOTS");
            break;
        case T_GOET:
            emit(g, "LTS");
            emit(g, "NOTS");
            break;
        default:
            g->error = 99;
            break;
    }
}

// any expression node: the result is left on the data stack
static void gen_expr(gen_t *g, const tree_node_t *node) {
    if (node->type == NODE_TERMINAL) {
        gen_term(g, node);
    } else if (node->rule == RULE_EXPRESSION_UNARY) {
        gen_unary(g, node);
    } else if (node->rule == RULE_EXPRESSION_AFTER_ID) {
        gen_call(g, node);
    } else if (node->rule == RULE_EXPRESSION && node->children_count == 1) {
        gen_expr(g, node->children[0]);
    } else if (node->rule == RULE_EXPRESSION && node->children_count == 3) {
        gen_binary(g, node);
    } else {
        emit(g, "# TODO expression");
        emit(g, "PUSHS nil@nil");
    }

    // implicit Int -> Double conversion marked by semantics
    const SemanticNodeInfo *i = semantic_info_find(g->info, node);
    if (i && i->promote_to_double) {
        emit(g, "INT2FLOATS");
    }
}

// VALUE: expression
static void gen_value(gen_t *g, const tree_node_t *value) {
    if (value->rule == RULE_VALUE_2 && value->children[1]->rule == RULE_MATCH_SUFFIX_2) {
        gen_expr(g, value->children[0]);
        return;
    }
    emit (g, "# TODO value");
    emit(g, "PUSHS nil@nil");
}

// RHS: value
static void gen_rhs(gen_t *g, const tree_node_t *rhs) {
    if (rhs->rule == RULE_RHS_2) {
        gen_value(g, rhs->children[0]);
        return;
    }
    emit(g, "# TODO block as value");
    emit(g, "PUSHS nil@nil");
}

// STATEMENTS
static void gen_inline(gen_t *g, const tree_node_t *node) {
    if (node->rule == RULE_INLINE_STATEMENT_5) {
        const tree_node_t *rest = node->children[node->children_count - 1];
        if (rest->rule == RULE_ID_STATEMENT_1) { // x = value
            gen_rhs(g, rest->children[1]);
            emit_var(g, "POPS", node->children[0]);
        } else { // expression starting with id, result is thrown away
            gen_expr(g, rest->children[0]);
            emit(g, "POPS GF@$t1");
        }
        return;
    }
    if (node->rule == RULE_INLINE_STATEMENT_6) { // expression as statement
        gen_expr(g, node->children[0]);
        emit(g, "POPS GF@$t1");
        return;
    }
    emit(g, "# TODO statement");
}

// STATEMENT_1 = val/var definition, STATEMENT_2 = other statements
static void gen_statement(gen_t *g, const tree_node_t *stmt) {
    if (stmt->rule == RULE_STATEMENT_1) {
        const tree_node_t *def = stmt->children[0]; // VAR_DEFINITION
        gen_rhs(g, def->children[4]);
        emit(g, "POPS LF@%s$%zu", decl_name(def), decl_id(g, def));
    } else {
        gen_inline(g, stmt->children[0]);
    }
}

// one line: statement
static void gen_statement_line(gen_t *g, const tree_node_t *line) {
    gen_statement(g, line->children[0]);
    for (const tree_node_t *r = line->children[1]; r->rule == RULE_STATEMENT_LINE_REST_1; r = r->children[2]) {
        gen_statement(g, r->children[1]);
    }
}

// block children: \n, INDENT, STATEMENT_LIST, DEDENT
static void gen_block(gen_t *g, const tree_node_t *block) {
    const tree_node_t *list = block->children[2];
    gen_statement_line(g, list->children[0]);
    for (const tree_node_t *r = list->children[1]; r->rule == RULE_STATEMENT_LIST_REST_1; r = r->children[1]) {
        gen_statement_line(g, r->children[0]);
    }
}

// FUNCTIONS
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

// FUNCTION_DEF children
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

    // body
    const tree_node_t *rhs = def->children[5];
    if (rhs->rule == RULE_RHS_1) {
        gen_block(g, rhs->children[0]);
    } else {
        gen_rhs(g, rhs);
        emit(g, "POPS GF@$t1");
    }

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
    emit(&g, "DEFVAR GF@$t1");
    emit(&g, "DEFVAR GF@$t2");
    emit(&g, "DEFVAR GF@$t3");
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