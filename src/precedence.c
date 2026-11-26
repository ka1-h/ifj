// project: IFJ26 compiler
// author: Karin Hlavatá <xhlavak00>

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
    PREC_DOT,
    PREC_COUNT
} prec_group_t;

static prec_group_t get_group(t_type type, bool expect_operand)
{
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
    case T_DOT:
        return PREC_DOT;
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
    tree_node_t* node;
} stack_item_t;

typedef struct {
    stack_item_t* items;
    size_t count;
    size_t capacity;
} prec_stack_t;

static bool stack_init(prec_stack_t* s)
{
    s->capacity = 16;
    s->count = 0;
    s->items = malloc(s->capacity * sizeof(stack_item_t));
    if (!s->items) {
        return false;
    }
    s->items[0] = (stack_item_t) { ITEM_TERM, PREC_DOLLAR, NULL };
    s->count = 1;
    return true;
}

static bool stack_push(prec_stack_t* s, stack_item_t item)
{
    if (s->count == s->capacity) {
        size_t new_capacity = s->capacity * 2;
        stack_item_t* tmp = realloc(s->items, new_capacity * sizeof(stack_item_t));
        if (!tmp) {
            return false;
        }
        s->items = tmp;
        s->capacity = new_capacity;
    }
    s->items[s->count++] = item;
    return true;
}

static size_t stack_top_terminal(const prec_stack_t* s)
{
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
static bool stack_insert_mark(prec_stack_t* s, size_t pos)
{
    if (!stack_push(s, (stack_item_t) { ITEM_MARK, PREC_DOLLAR, NULL })) {
        return false;
    }
    size_t move = s->count - 1 - (pos + 1);
    memmove(&s->items[pos + 2], &s->items[pos + 1], move * sizeof(stack_item_t));
    s->items[pos + 1] = (stack_item_t) { ITEM_MARK, PREC_DOLLAR, NULL };
    return true;
}

static void stack_free(prec_stack_t* s)
{
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
    // unary muldiv add rel eq and or term lpar rpar dollar dot
    { R_LT, R_GT, R_GT, R_GT, R_GT, R_GT, R_GT, R_LT, R_LT, R_GT, R_GT, R_LT }, // unary
    { R_LT, R_GT, R_GT, R_GT, R_GT, R_GT, R_GT, R_LT, R_LT, R_GT, R_GT, R_LT }, // muldiv
    { R_LT, R_LT, R_GT, R_GT, R_GT, R_GT, R_GT, R_LT, R_LT, R_GT, R_GT, R_LT }, // add
    { R_LT, R_LT, R_LT, R_ERR, R_GT, R_GT, R_GT, R_LT, R_LT, R_GT, R_GT, R_LT }, // rel
    { R_LT, R_LT, R_LT, R_LT, R_ERR, R_GT, R_GT, R_LT, R_LT, R_GT, R_GT, R_LT }, // eq
    { R_LT, R_LT, R_LT, R_LT, R_LT, R_GT, R_GT, R_LT, R_LT, R_GT, R_GT, R_LT }, // and
    { R_LT, R_LT, R_LT, R_LT, R_LT, R_LT, R_GT, R_LT, R_LT, R_GT, R_GT, R_LT }, // or
    { R_ERR, R_GT, R_GT, R_GT, R_GT, R_GT, R_GT, R_ERR, R_ERR, R_GT, R_GT, R_GT }, // term
    { R_LT, R_LT, R_LT, R_LT, R_LT, R_LT, R_LT, R_LT, R_LT, R_EQ, R_ERR, R_LT }, // lpar
    { R_ERR, R_GT, R_GT, R_GT, R_GT, R_GT, R_GT, R_ERR, R_ERR, R_GT, R_GT, R_GT }, // rpar
    { R_LT, R_LT, R_LT, R_LT, R_LT, R_LT, R_LT, R_LT, R_LT, R_ERR, R_ERR, R_LT }, // dollar
    { R_ERR, R_GT, R_GT, R_GT, R_GT, R_GT, R_GT, R_LT, R_ERR, R_GT, R_GT, R_GT }, // dot
};

// sets error code, if no error happened before
static void set_error(Parser* p, int code)
{
    if (p->error == PARSE_OK) {
        p->error = code;
    }
}

// true for groups of binary operators
static bool is_binary_op(prec_group_t g)
{
    return g == PREC_MULDIV || g == PREC_ADD || g == PREC_REL || g == PREC_EQ || g == PREC_AND || g == PREC_OR || g == PREC_DOT;
}

// moves the subtree of a stack item under parent
static bool attach_item(tree_node_t* parent, stack_item_t* item)
{
    if (!tree_add_child(parent, item->node)) {
        return false;
    }
    item->node = NULL;
    return true;
}

static bool is_member_name(const tree_node_t* e)
{
    if (!e || e->type != NODE_NONTERMINAL || e->rule != RULE_EXPRESSION || e->children_count != 1) {
        return false;
    }
    const tree_node_t* c = e->children[0];
    if (c->type == NODE_TERMINAL) {
        return c->token.type == T_ID;
    }
    return c->rule == RULE_EXPRESSION_AFTER_ID;
}

// reduces the handle on top of the stack to one nonterminal E
static bool reduce(Parser* p, prec_stack_t* s)
{
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

    stack_item_t* h = &s->items[mark + 1];
    size_t n = s->count - mark - 1;
    tree_node_t* result = NULL;
    bool ok = true;

    if (n == 1 && h[0].kind == ITEM_TERM && h[0].group == PREC_TERM) {
        // E -> i
        result = tree_create_nonterminal(RULE_EXPRESSION);
        ok = result && attach_item(result, &h[0]);
    } else if (n == 2 && h[0].kind == ITEM_TERM && h[0].group == PREC_UNARY && h[1].kind == ITEM_NONTERM) {
        // E -> -E, E -> !E
        result = tree_create_nonterminal(RULE_EXPRESSION_UNARY);
        ok = result && attach_item(result, &h[0]) && attach_item(result, &h[1]);
    } else if (n == 3 && h[0].kind == ITEM_NONTERM && h[1].kind == ITEM_TERM && is_binary_op(h[1].group) && h[2].kind == ITEM_NONTERM && (h[1].group != PREC_DOT || is_member_name(h[2].node))) {
        // E -> E op E, E -> E.name
        result = tree_create_nonterminal(RULE_EXPRESSION);
        ok = result && attach_item(result, &h[0]) && attach_item(result, &h[1]) && attach_item(result, &h[2]);
    } else if (n == 3 && h[0].kind == ITEM_TERM && h[0].group == PREC_LPAR && h[1].kind == ITEM_NONTERM && h[2].kind == ITEM_TERM && h[2].group == PREC_RPAR) {
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
    s->items[s->count++] = (stack_item_t) { ITEM_NONTERM, PREC_DOLLAR, result };
    return true;
}

// pushes current input token on stack (as a leaf) and reads the next one
static bool shift(Parser* p, prec_stack_t* s, prec_group_t group)
{
    tree_node_t* leaf = tree_create_terminal(&p->current);
    if (!leaf || !stack_push(s, (stack_item_t) { ITEM_TERM, group, leaf })) {
        tree_dispose(&leaf);
        set_error(p, PARSE_ERR_INTERNAL);
        return false;
    }
    return parser_advance(p);
}

// reads the arguments of a call
static void parse_arg_list(Parser* p, tree_node_t* call)
{
    tree_node_t* args = tree_create_nonterminal(RULE_CALL_ARGS);
    if (!args || !tree_add_child(call, args)) {
        tree_dispose(&args);
        set_error(p, PARSE_ERR_INTERNAL);
        return;
    }
    if (parser_check(p, T_UNIT_BRACKETS)) {
        parser_expect(p, T_UNIT_BRACKETS, args);
        return;
    }
    parser_expect(p, T_LPARENTH, args);
    if (!parser_check(p, T_RPARENTH)) {
        rule_value(p, args);
        while (p->error == PARSE_OK && parser_check(p, T_COMMA)) {
            parser_expect(p, T_COMMA, args);
            // trailing comma: allowed only if ')' follows on a new line
            if (parser_check(p, T_RPARENTH) && p->newline_before) {
                break;
            }
            rule_value(p, args);
        }
    }
    parser_expect(p, T_RPARENTH, args);
}

// second half of shift_id - identifier lead is already read
static bool push_id_or_call(Parser* p, prec_stack_t* s, tree_node_t* node)
{
    if (parser_check(p, T_LPARENTH) || parser_check(p, T_UNIT_BRACKETS)) {
        tree_node_t* call = tree_create_nonterminal(RULE_EXPRESSION_AFTER_ID);
        if (!call || !tree_add_child(call, node)) {
            tree_dispose(&node);
            tree_dispose(&call);
            set_error(p, PARSE_ERR_INTERNAL);
            return false;
        }
        node = call;
        while (p->error == PARSE_OK && (parser_check(p, T_LPARENTH) || parser_check(p, T_UNIT_BRACKETS))) {
            parse_arg_list(p, node);
        }
        if (p->error != PARSE_OK) {
            tree_dispose(&node);
            return false;
        }
    }
    if (!stack_push(s, (stack_item_t) { ITEM_TERM, PREC_TERM, node })) {
        tree_dispose(&node);
        set_error(p, PARSE_ERR_INTERNAL);
        return false;
    }
    return true;
}

// pushes an identifier on the stack. if "(" follows, it is a function call -> reads all its arguments and pushes call as one term
static bool shift_id(Parser* p, prec_stack_t* s)
{
    tree_node_t* node = tree_create_terminal(&p->current);
    if (!node) {
        set_error(p, PARSE_ERR_INTERNAL);
        return false;
    }
    if (!parser_advance(p)) {
        tree_dispose(&node);
        return false;
    }
    return push_id_or_call(p, s, node);
}

static void parse_expression(Parser* p, tree_node_t* parent, tree_node_t* first_lpar, tree_node_t* first_term)
{
    if (p->error != PARSE_OK) {
        tree_dispose(&first_lpar);
        tree_dispose(&first_term);
        return;
    }

    prec_stack_t s;
    if (!stack_init(&s)) {
        tree_dispose(&first_lpar);
        tree_dispose(&first_term);
        set_error(p, PARSE_ERR_INTERNAL);
        return;
    }

    bool expect_operand = true;
    int paren_depth = 0;

    if (first_lpar) {
        if (stack_insert_mark(&s, stack_top_terminal(&s)) && stack_push(&s, (stack_item_t) { ITEM_TERM, PREC_LPAR, first_lpar })) {
            paren_depth = 1;
        } else {
            tree_dispose(&first_lpar);
            set_error(p, PARSE_ERR_INTERNAL);
        }
    }

    if (first_term) {
        bool ok;
        if (p->error != PARSE_OK || !stack_insert_mark(&s, stack_top_terminal(&s))) {
            tree_dispose(&first_term);
            set_error(p, PARSE_ERR_INTERNAL);
            ok = false;
        } else if (first_term->token.type == T_ID) {
            ok = push_id_or_call(p, &s, first_term);
        } else {
            ok = stack_push(&s, (stack_item_t) { ITEM_TERM, PREC_TERM, first_term });
            if (!ok) {
                tree_dispose(&first_term);
                set_error(p, PARSE_ERR_INTERNAL);
            }
        }
        if (ok) {
            expect_operand = false;
        }
    }

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
        bool shifted = (b == PREC_TERM && p->current.type == T_ID) ? shift_id(p, &s) : shift(p, &s, b);
        if (!shifted) {
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

void rule_expression(Parser* p, tree_node_t* parent)
{
    parse_expression(p, parent, NULL, NULL);
}
void rule_expression_after_id(Parser* p, tree_node_t* parent, tree_node_t* id_leaf)
{
    parse_expression(p, parent, NULL, id_leaf);
}

// creates a nonterminal and attaches it to parent
static tree_node_t* new_child(Parser* p, tree_node_t* parent, tree_rule_t rule)
{
    tree_node_t* node = tree_create_nonterminal(rule);
    if (!node || !tree_add_child(parent, node)) {
        tree_dispose(&node);
        set_error(p, PARSE_ERR_INTERNAL);
        return NULL;
    }
    return node;
}

// attaches a leaf to parent, frees it on failure
static bool add_leaf(Parser* p, tree_node_t* parent, tree_node_t* leaf)
{
    if (!tree_add_child(parent, leaf)) {
        tree_dispose(&leaf);
        set_error(p, PARSE_ERR_INTERNAL);
        return false;
    }
    return true;
}

// makes a leaf from the current token and moves to the next one
static tree_node_t* take_leaf(Parser* p)
{
    tree_node_t* leaf = tree_create_terminal(&p->current);
    if (!leaf) {
        set_error(p, PARSE_ERR_INTERNAL);
        return NULL;
    }
    if (!parser_advance(p)) {
        tree_dispose(&leaf);
        return NULL;
    }
    return leaf;
}

// LAMBDA_ARROW := '=>' RHS (rule 65)
static void parse_lambda_arrow(Parser* p, tree_node_t* parent)
{
    if (p->error != PARSE_OK)
        return;
    tree_node_t* node = new_child(p, parent, RULE_LAMBDA_ARROW);
    if (!node)
        return;
    parser_expect(p, T_LAMB, node);
    rule_rhs(p, node);
}

// LAMBDA_AFTER_ID := ':' TYPE PARAM_LIST_REST ')' LAMBDA_ARROW (rule 64)
static void parse_lambda_after_id(Parser* p, tree_node_t* parent)
{
    if (p->error != PARSE_OK)
        return;
    tree_node_t* node = new_child(p, parent, RULE_LAMBDA_AFTER_ID);
    if (!node)
        return;
    parser_expect(p, T_COL, node);
    rule_type(p, node);
    rule_param_list_rest(p, node);
    parser_expect(p, T_RPARENTH, node);
    parse_lambda_arrow(p, node);
}

void rule_value_expression(Parser* p, tree_node_t* parent)
{
    if (p->error != PARSE_OK)
        return;

    // "() => body", or just the Unit value "()"
    if (parser_check(p, T_UNIT_BRACKETS)) {
        tree_node_t* unit = take_leaf(p);
        if (!unit)
            return;
        if (!parser_check(p, T_LAMB)) {
            parse_expression(p, parent, NULL, unit);
            return;
        }
        tree_node_t* lambda = new_child(p, parent, RULE_VALUE_EXPRESSION);
        if (!lambda) {
            tree_dispose(&unit);
            return;
        }
        if (add_leaf(p, lambda, unit)) {
            parse_lambda_arrow(p, lambda);
        }
        return;
    }

    // "(identif: Type, ...) => body", or an expression starting with "("
    if (parser_check(p, T_LPARENTH)) {
        tree_node_t* lpar = take_leaf(p);
        if (!lpar)
            return;
        if (!parser_check(p, T_ID)) {
            parse_expression(p, parent, lpar, NULL);
            return;
        }
        tree_node_t* id = take_leaf(p);
        if (!id) {
            tree_dispose(&lpar);
            return;
        }
        if (!parser_check(p, T_COL)) {
            parse_expression(p, parent, lpar, id);
            return;
        }
        // "(" identif ":" -> it is a lambda
        tree_node_t* lambda = new_child(p, parent, RULE_VALUE_EXPRESSION);
        if (!lambda) {
            tree_dispose(&lpar);
            tree_dispose(&id);
            return;
        }
        if (!add_leaf(p, lambda, lpar)) {
            tree_dispose(&id);
            return;
        }
        if (add_leaf(p, lambda, id)) {
            parse_lambda_after_id(p, lambda);
        }
        return;
    }
    rule_expression(p, parent);
}