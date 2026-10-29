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

// sets error code, if no error happened before
static void set_error(Parser *p, int code) {
    if (p->error == PARSE_OK) {
        p->error = code;
    }
}

// true for groups of binary operators
static bool is_binary_op(prec_group_t g) {
    return g == PREC_MULDIV || g == PREC_ADD || g == PREC_REL || g == PREC_EQ || g == PREC_AND || g == PREC_OR;
}

// moves the subtree of a stack item under parent
static bool attach_item(tree_node_t *parent, stack_item_t *item) {
    if (!tree_add_child(parent, item->node)) {
        return false;
    }
    item->node = NULL;
    return true;
}

// reduces the handle on top of the stack to one nonterminal E
static bool reduce(Parser *p, prec_stack_t *s) {
    size_t mark = s->count;
    while (mark > 0) {
        mark--;
        if (s->items[mark].kind == ITEM_MARK) {
            break;
        }
    }
    if (s->items[mark].kind != ITEM_MARK) {
        set_error(p, PARSE_ERR_SYNTAX);
        return false;
    }

    stack_item_t *h = &s->items[mark + 1];
    size_t n = s->count - mark - 1;
    tree_node_t *result = NULL;
    bool ok = true;

    if (n == 1 && h[0].kind == ITEM_TERM && h[0].group == PREC_TERM) {
        // E -> i
        result = tree_create_nonterminal(RULE_EXPRESSION);
        ok = result && attach_item(result, &h[0]);
    } else if (n == 2 && h[0].kind == ITEM_TERM && h[0].group == PREC_UNARY && h[1].kind == ITEM_NONTERM) {
        // E -> -E, E -> !E
        result = tree_create_nonterminal(RULE_EXPRESSION_UNARY);
        ok = result && attach_item(result, &h[0]) && attach_item(result, &h[1]);
    } else if (n == 3 && h[0].kind == ITEM_NONTERM && h[1].kind == ITEM_TERM && is_binary_op(h[1].group) && h[2].kind == ITEM_NONTERM) {
        // E -> E op E
        result = tree_create_nonterminal(RULE_EXPRESSION);
        ok = result && attach_item(result, &h[0]) && attach_item(result, &h[1]) && attach_item(result, &h[2]); 
    } else if (n == 3 && h[0].kind == ITEM_TERM && h[0].group == PREC_LPAR && h[1].kind == ITEM_NONTERM &&
        h[2].kind == ITEM_TERM && h[2].group == PREC_RPAR) {
        // E -> (E)
        result = h[1].node;
        h[1].node = NULL;
        tree_dispose(&h[0].node);
        tree_dispose(&h[2].node);
    } else {
        set_error(p, PARSE_ERR_SYNTAX);
        return false;
    }
    if (!ok) {
        tree_dispose(&result);
        set_error(p, PARSE_ERR_INTERNAL);
        return false;
    }
    // replace "< handle" with E
    s->count = mark;
    s->items[s->count++] = (stack_item_t){ITEM_NONTERM, PREC_DOLLAR, result};
    return true;
}

// pushes current input token on stack (as a leaf) and reads the next one
static bool shift(Parser *p, prec_stack_t *s, prec_group_t group) {
    tree_node_t *leaf = tree_create_terminal(&p->current);
    if (!leaf || !stack_push(s, (stack_item_t){ITEM_TERM, group, leaf})) {
        tree_dispose(&leaf);
        set_error(p, PARSE_ERR_INTERNAL);
        return false;
    }
    return parser_advance(p);
}

void rule_expression(Parser *p, tree_node_t *parent) {
    if (p->error != PARSE_OK) {
        return;
    }

    prec_stack_t s;
    if (!stack_init(&s)) {
        set_error(p, PARSE_ERR_INTERNAL);
        return;
    }

    bool expect_operand = true;
    int paren_depth = 0;

    while (p->error == PARSE_OK) {
        // b = group of the current input token
        prec_group_t b = get_group(p->current.type, expect_operand);
        if (b == PREC_RPAR && paren_depth == 0) {
            b = PREC_DOLLAR;
        }

        // a = group of the topmost terminal on the stack
        size_t top = stack_top_terminal(&s);
        prec_group_t a = s.items[top].group;

        // end: input is $ and stack is "$ E"
        if (a == PREC_DOLLAR && b == PREC_DOLLAR) {
            if (s.count != 2) { // only "$" = empty expression
                set_error(p, PARSE_ERR_SYNTAX);
            }
            break;
        }

        prec_rel_t rel = prec_table[a][b];

        if (rel == R_GT) {
            reduce(p, &s);
            continue;
        }
        if (rel == R_ERR) {
            set_error(p, PARSE_ERR_SYNTAX);
            break;
        }

        // R_LT or R_EQ: shift; R_LT also inserts the mark first
        if (rel == R_LT && !stack_insert_mark(&s, top)) {
            set_error(p, PARSE_ERR_INTERNAL);
            break;
        }
        if (!shift(p, &s, b)) {
            break;
        }

        if (b == PREC_LPAR) {
            paren_depth++;
        } else if (b == PREC_RPAR) {
            paren_depth--;
        }
        expect_operand = !(b == PREC_TERM || b == PREC_RPAR);
    }

    // success - move final E under parent
    if (p->error == PARSE_OK) {
        if (tree_add_child(parent, s.items[1].node)) {
            s.items[1].node = NULL;
        } else {
            set_error(p, PARSE_ERR_INTERNAL);
        }
    }
    stack_free(&s);
}