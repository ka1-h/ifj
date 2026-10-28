#include "precedence.h"
#include <stdlib.h>
#include <string.h>

typedef enum {
    PREC_UNARY, // unary -, !
    PREC_MULDIV, // *, /
    PREC_ADD, // +, binary -
    PREC_REL, // <, <=, >, >=
    PREC_EQ, // ==, !=
    PREC_AND, // &&
    PREC_OR, // ||
    PREC_TERM, // id, literal, (), true, false
    PREC_LPAR, // (
    PREC_RPAR, // )
    PREC_DOLLAR, // anything else (end of input)
    PREC_COUNT
} prec_group_t;

static prec_group_t get_group(t_type type, bool expect_operand) {
    switch (type) {
        case T_MINUS:
            return expect_operand ? PREC_UNARY : PREC_ADD;
        case T_NEG:
            return PREC_UNARY;
        case T_MULT:
        case T_DIV:
            return PREC_MULDIV;
        case T_PLUS:
            return PREC_ADD;
        case T_LT:
        case T_LOET:
        case T_GT:
        case T_GOET:
            return PREC_REL;
        case T_ET:
        case T_NET:
            return PREC_EQ;
        case T_AND:
            return PREC_AND;
        case T_OR:
            return PREC_OR;
        case T_ID:
        case T_STRING:
        case T_INT:
        case T_DOUBLE:
        case T_UNIT_BRACKETS:
        case KW_TRUE:
        case KW_FALSE:
            return PREC_TERM;
        case T_LPARENTH:
            return PREC_LPAR;
        case T_RPARENTH:
            return PREC_RPAR;
        default:
            return PREC_DOLLAR;
    }
}

typedef enum {
    ITEM_TERM, 
    ITEM_NONTERM,
    ITEM_MARK
} item_kind_t;

typedef struct {
    item_kind_t kind;
    prec_group_t group;
    tree_node_t *node;
} stack_item_t;

typedef struct {
    stack_item_t *items;
    size_t count;
    size_t capacity;
} prec_stack_t;

static bool stack_init(prec_stack_t *s) {
    s->capacity = 16;
    s->count = 0;
    s->items = malloc(s->capacity * sizeof(stack_item_t));
    if (!s->items) {
        return false;
    }
    s->items[0] = (stack_item_t){ITEM_TERM, PREC_DOLLAR, NULL};
    s->count = 1;
    return true;
}

static bool stack_push(prec_stack_t *s, stack_item_t item) {
    if (s->count == s->capacity) {
        size_t new_capacity = s->capacity * 2;
        stack_item_t *tmp = realloc(s->items, new_capacity * sizeof(stack_item_t));
        if (!tmp) {
            return false;
        } 
        s->items = tmp;
        s->capacity = new_capacity;
    }
    s->items[s->count++] = item;
    return true;
}

static size_t stack_top_terminal(const prec_stack_t *s) {
    size_t i = s->count;
    while (i > 0) {
        i--;
        if (s->items[i].kind == ITEM_TERM) {
            return i;
        }
    }
    return 0;
}

// < - start of handle
static bool stack_insert_mark(prec_stack_t *s, size_t pos) {
    if (!stack_push(s, (stack_item_t){ITEM_MARK, PREC_DOLLAR, NULL})) {
        return false;
    }
    size_t move = s->count - 1 - (pos + 1);
    memmove(&s->items[pos + 2], &s->items[pos + 1], move * sizeof(stack_item_t));
    s->items[pos + 1] = (stack_item_t){ITEM_MARK, PREC_DOLLAR, NULL};
    return true;   
}

static void stack_free(prec_stack_t *s) {
    for (size_t i = 0; i < s->count; i++) {
        tree_dispose(&s->items[i].node);
    }
    free(s->items);
    s->items = NULL;
    s->count = 0;
    s->capacity = 0;
}

typedef enum {
    R_LT, // < - insert mark, push token
    R_GT, // > - reduce
    R_EQ, // = - push token without mark
    R_ERR // empty (syntax error)
} prec_rel_t;

static const prec_rel_t prec_table[PREC_COUNT][PREC_COUNT] = {
    // unary muldiv add rel eq and or term lpar rpar dollar
    {R_LT, R_GT, R_GT, R_GT, R_GT, R_GT, R_GT, R_LT, R_LT, R_GT, R_GT}, // unary
    {R_LT, R_GT, R_GT, R_GT, R_GT, R_GT, R_GT, R_LT, R_LT, R_GT, R_GT}, // muldiv
    {R_LT, R_LT, R_GT, R_GT, R_GT, R_GT, R_GT, R_LT, R_LT, R_GT, R_GT}, // add
    {R_LT, R_LT, R_LT, R_ERR, R_GT, R_GT, R_GT, R_LT, R_LT, R_GT, R_GT}, // rel
    {R_LT, R_LT, R_LT, R_LT, R_ERR, R_GT, R_GT, R_LT, R_LT, R_GT, R_GT}, // eq
    {R_LT, R_LT, R_LT, R_LT, R_LT, R_GT, R_GT, R_LT, R_LT, R_GT, R_GT}, // and
    {R_LT, R_LT, R_LT, R_LT, R_LT, R_LT, R_GT, R_LT, R_LT, R_GT, R_GT}, // or
    {R_ERR, R_GT, R_GT, R_GT, R_GT, R_GT, R_GT, R_ERR, R_ERR, R_GT, R_GT}, // term
    {R_LT, R_LT, R_LT, R_LT, R_LT, R_LT, R_LT, R_LT, R_LT, R_EQ, R_ERR}, // lpar
    {R_ERR, R_GT, R_GT, R_GT, R_GT, R_GT, R_GT, R_ERR, R_ERR, R_GT, R_GT}, // rpar
    {R_LT, R_LT, R_LT, R_LT, R_LT, R_LT, R_LT, R_LT, R_LT, R_ERR, R_ERR},  // dollar
};