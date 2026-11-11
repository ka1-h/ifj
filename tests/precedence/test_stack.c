// Quick test of precedence stack functions
#include "../../src/precedence.c"

static void print_stack(const prec_stack_t *s) {
    for (size_t i = 0; i < s->count; i++) {
        const stack_item_t *it = &s->items[i];
        if (it->kind == ITEM_MARK)
            printf("< ");
        else if (it->kind == ITEM_NONTERM)
            printf("E ");
        else if (it->group == PREC_DOLLAR)
            printf("$ ");
        else if (it->group == PREC_ADD)
            printf("+ ");
        else
            printf("i ");
    }
    printf("   (top terminal index: %zu)\n", stack_top_terminal(s));
}

int main(void) {
    prec_stack_t s;
    if (!stack_init(&s)) return 1;
    print_stack(&s);  // $

    stack_push(&s, (stack_item_t){ITEM_NONTERM, PREC_DOLLAR, NULL});
    print_stack(&s);  // $ E

    stack_insert_mark(&s, stack_top_terminal(&s));
    print_stack(&s);  // $ < E

    stack_push(&s, (stack_item_t){ITEM_TERM, PREC_ADD, NULL});
    print_stack(&s);  // $ < E +

    stack_insert_mark(&s, stack_top_terminal(&s));
    stack_push(&s, (stack_item_t){ITEM_TERM, PREC_TERM, NULL});
    print_stack(&s);  // $ < E + < i

    for (int i = 0; i < 30; i++)
        stack_push(&s, (stack_item_t){ITEM_NONTERM, PREC_DOLLAR, NULL});
    printf("count after 30 pushes: %zu\n", s.count);  // 36

    stack_free(&s);
    return 0;
}