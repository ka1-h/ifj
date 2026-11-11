// Quick test of reduce(): builds the stack by hand like the main loop would
#include "../../src/precedence.c"

static void push_term(prec_stack_t *s, prec_group_t group, t_type type, char *text) {
    Token t = {type, text, 1};
    stack_push(s, (stack_item_t){ITEM_TERM, group, tree_create_terminal(&t)});
}

static void push_mark(prec_stack_t *s) {
    stack_insert_mark(s, stack_top_terminal(s));
}

int main(void) {
    Parser p = {0};
    p.error = PARSE_OK;
    prec_stack_t s;

    // ---- test 1: a + b ----
    stack_init(&s);
    push_mark(&s); push_term(&s, PREC_TERM, T_ID, "a");   // $ < a
    reduce(&p, &s);                                        // $ E
    push_mark(&s); push_term(&s, PREC_ADD, T_PLUS, "+");   // $ < E +
    push_mark(&s); push_term(&s, PREC_TERM, T_ID, "b");   // $ < E + < b
    reduce(&p, &s);                                        // $ < E + E
    reduce(&p, &s);                                        // $ E
    printf("test 1 (a + b): error=%d, count=%zu\n", p.error, s.count);
    tree_print(s.items[1].node, stdout);
    stack_free(&s);

    // ---- test 2: (x) ----
    stack_init(&s);
    push_mark(&s); push_term(&s, PREC_LPAR, T_LPARENTH, "(");  // $ < (
    push_mark(&s); push_term(&s, PREC_TERM, T_ID, "x");        // $ < ( < x
    reduce(&p, &s);                                             // $ < ( E
    push_term(&s, PREC_RPAR, T_RPARENTH, ")");                  // $ < ( E )
    reduce(&p, &s);                                             // $ E
    printf("\ntest 2 ((x)): error=%d, count=%zu\n", p.error, s.count);
    tree_print(s.items[1].node, stdout);
    stack_free(&s);

    // ---- test 3: -y ----
    stack_init(&s);
    push_mark(&s); push_term(&s, PREC_UNARY, T_MINUS, "-");  // $ < -
    push_mark(&s); push_term(&s, PREC_TERM, T_ID, "y");      // $ < - < y
    reduce(&p, &s);                                           // $ < - E
    reduce(&p, &s);                                           // $ E
    printf("\ntest 3 (-y): error=%d, count=%zu\n", p.error, s.count);
    tree_print(s.items[1].node, stdout);
    stack_free(&s);

    // ---- test 4: invalid handle "E b" must fail with error 2 ----
    stack_init(&s);
    push_mark(&s); push_term(&s, PREC_TERM, T_ID, "a");
    reduce(&p, &s);                                           // $ E
    push_mark(&s);                                            // $ < E
    push_term(&s, PREC_TERM, T_ID, "b");
    reduce(&p, &s);                                           // "< E b" is no rule
    printf("\ntest 4 (a b): error=%d (expected 2)\n", p.error);
    stack_free(&s);

    return 0;
}