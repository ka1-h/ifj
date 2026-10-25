// vsetky tokeny: T_ID, T_STRING,T_INT, T_DOUBLE, T_UNIT_BRACKETS, T_LPARENTH, T_RPARENTH, T_PLUS, T_MINUS, T_MULT, T_DIV, T_LT, T_LOET
// T_GT, T_GOET, T_ET, T_NET, KW_TRUE, KW_FALSE, T_NEG, T_AND, T_OR

// priorita operatory     tokeny                          nazov skupiny
// 1        unarne -, !   T_MINUS, T_NEG                  PREC_UNARY
// 2        *, /          T_MULT, T_DIV                   PREC_MULDIV
// 3        +, -          T_PLUS, T_MINUS                 PREC_ADD
// 4        <, <=, >, >=  T_LT, T_LOET, T_GT, T_GOET      PREC_REL
// 5        ==, !=        T_ET, T_NET                     PREC_EQ
// 6        &&            T_AND                           PREC_AND
// 7        ||            T_OR                            PREC_OR

// termy: T_ID, T_STRING, T_INT, T_DOUBLE, T_UNIT_BRACKETS, KW_TRUE, KW_FALSE     PREC_TERM
// T_LPARENTH                                                                     PREC_LPAR
// T_RPARENTH                                                                     PREC_RPAR
// $                                                                              PREC_DOLLAR

// unary   muldiv   add    rel     eq      and     or
//   =      <       

#include "precedence.h"

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