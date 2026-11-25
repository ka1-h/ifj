// project: IFJ26 compiler
// author: Karin Hlavatá <xhlavak00>

#include "generator.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// array of tree nodes
typedef struct {
    const tree_node_t** items;
    size_t count;
    size_t capacity;
} node_list_t;

typedef struct {
    FILE* out;
    const SemanticInfo* info;
    node_list_t decls;
    int error;
    int labels; // counter for unique label numbers
} gen_t;

// writes one line of code
static void emit(gen_t* g, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(g->out, fmt, args);
    va_end(args);
    fputc('\n', g->out);
}

// appends a node to the list (sets error 99 on failure)
static void list_push(gen_t* g, node_list_t* l, const tree_node_t* node)
{
    if (l->count == l->capacity) {
        size_t cap = l->capacity ? l->capacity * 2 : 16;
        const tree_node_t** tmp = realloc(l->items, cap * sizeof(*tmp));
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
static size_t decl_id(gen_t* g, const tree_node_t* decl)
{
    for (size_t i = 0; i < g->decls.count; i++) {
        if (g->decls.items[i] == decl) {
            return i;
        }
    }
    list_push(g, &g->decls, decl);
    return g->decls.count - 1;
}

// name in the source: VAR_DEFINITION has it as child 1, PARAM as child 0
static const char* decl_name(const tree_node_t* decl)
{
    int idx = (decl->rule == RULE_VAR_DEFINITION) ? 1 : 0;
    return decl->children[idx]->token.data;
}

// FUNCTION_DEF children
static const char* function_name(const tree_node_t* def)
{
    return def->children[1]->token.data;
}

// EXTENSION_DEF that contains a method, or NULL for a normal function
static const tree_node_t* method_extension(const tree_node_t* def)
{
    for (const tree_node_t* n = def->parent; n; n = n->parent) {
        if (n->rule == RULE_EXTENSION_DEF) {
            return n;
        }
    }
    return NULL;
}

// finds all nodes with the given rule in the subtree
static void collect(gen_t* g, const tree_node_t* node, tree_rule_t rule, node_list_t* out)
{
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
static DataType expr_type(gen_t* g, const tree_node_t* node)
{
    const SemanticNodeInfo* i = semantic_info_find(g->info, node);
    if (!i) {
        return TYPE_UNDEF;
    }
    return i->promote_to_double ? TYPE_DOUBLE : i->type;
}

// writes "<instr> LF@name$id" for the variable an identifier leaf refers to
static void emit_var(gen_t* g, const char* instr, const tree_node_t* id_leaf)
{
    const SemanticNodeInfo* i = semantic_info_find(g->info, id_leaf);
    if (!i || !i->declaration) {
        g->error = 99;
        return;
    }
    emit(g, "%s LF@%s$%zu", instr, decl_name(i->declaration), decl_id(g, i->declaration));
}

// LITERALS
// writes an Int literal without underscores
static void emit_int(gen_t* g, const char* text)
{
    fputs("PUSHS int@", g->out);
    for (const char* c = text; *c; c++) {
        if (*c != '_') {
            fputc(*c, g->out);
        }
    }
    fputc('\n', g->out);
}

// writes a double literal
static void emit_double(gen_t* g, const char* text)
{
    char buf[256];
    size_t n = 0;
    for (const char* c = text; *c && n < sizeof(buf) - 1; c++) {
        if (*c != '_' && *c != 'd' && *c != 'D') {
            buf[n++] = *c;
        }
    }
    buf[n] = '\0';
    emit(g, "PUSHS float@%a", strtod(buf, NULL));
}

// one character of a string constant
static void put_char(gen_t* g, unsigned char c)
{
    if (c <= 32 || c == '#' || c == '\\') {
        fprintf(g->out, "\\%03d", c);
    } else {
        fputc(c, g->out);
    }
}

// writes a string literal
static void emit_string(gen_t* g, const char* text)
{
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
                switch (text[i]) {
                case 'n':
                    c = '\n';
                    break;
                case 't':
                    c = '\t';
                    break;
                case 'r':
                    c = '\r';
                    break;
                default:
                    c = text[i];
                    break;
                }
            }
            put_char(g, (unsigned char)c);
        }
    }
    fputc('\n', g->out);
}

// EXRESSIONS
static void gen_expr(gen_t* g, const tree_node_t* node);
static void gen_if(gen_t* g, const tree_node_t* node, bool keep);
static void gen_match(gen_t* g, const tree_node_t* node, bool keep);

static void gen_value(gen_t* g, const tree_node_t* value)
{
    if (value->rule == RULE_VALUE_1) { // if as a value
        gen_if(g, value->children[0], true);
        return;
    }
    if (value->children[1]->rule == RULE_MATCH_SUFFIX_1) {
        gen_match(g, value, true);
        return;
    }
    if (value->rule == RULE_VALUE_2 && value->children[1]->rule == RULE_MATCH_SUFFIX_2) {
        gen_expr(g, value->children[0]);
        return;
    }
    emit(g, "# TODO value");
    emit(g, "PUSHS nil@nil");
}

// identifier: variable, or a function without parameters called without parentheses (PARAMLISTS)
static void gen_id(gen_t* g, const tree_node_t* leaf)
{
    const SemanticNodeInfo* i = semantic_info_find(g->info, leaf);
    if (i && i->declaration && i->declaration->rule == RULE_FUNCTION_DEF) {
        emit(g, "CREATEFRAME");
        emit(g, "CALL $f_%s", leaf->token.data);
        return;
    }
    emit_var(g, "PUSHS", leaf);
}

// leaf: variable or literal
static void gen_term(gen_t* g, const tree_node_t* leaf)
{
    switch (leaf->token.type) {
    case T_ID:
        gen_id(g, leaf);
        break;
    case T_INT:
        emit_int(g, leaf->token.data);
        break;
    case T_DOUBLE:
        emit_double(g, leaf->token.data);
        break;
    case T_STRING:
        emit_string(g, leaf->token.data);
        break;
    case T_UNIT_BRACKETS:
        emit(g, "PUSHS nil@nil");
        break;
    case KW_TRUE:
        emit(g, "PUSHS bool@true");
        break;
    case KW_FALSE:
        emit(g, "PUSHS bool@false");
        break;
    default:
        g->error = 99;
        break;
    }
}

// n-th argument (VALUE node) of a CALL_ARGS node, or NULL
static const tree_node_t* call_arg(const tree_node_t* args, size_t n)
{
    for (size_t i = 0; i < args->children_count; i++) {
        if (args->children[i]->type == NODE_NONTERMINAL && n-- == 0) {
            return args->children[i];
        }
    }
    return NULL;
}

// pushes all arguments of all argument lists, left to right
static void push_args(gen_t* g, const tree_node_t* call)
{
    for (size_t l = 1; l < call->children_count; l++) {
        const tree_node_t* args = call->children[l];
        for (size_t i = 0; i < args->children_count; i++) {
            if (args->children[i]->type == NODE_NONTERMINAL) {
                gen_value(g, args->children[i]);
            }
        }
    }
}

// type of the first argument of a call
static DataType first_arg_type(gen_t* g, const tree_node_t* call)
{
    const tree_node_t* arg = call_arg(call->children[1], 0);
    return arg ? expr_type(g, arg->children[0]) : TYPE_UNDEF;
}

// moved from toStr: converts the value on the stack to a string
static void emit_to_string(gen_t* g, DataType type)
{
    switch (type) {
    case TYPE_INT:
        emit(g, "INT2STRS");
        break;
    case TYPE_DOUBLE:
        emit(g, "FLOAT2STRS");
        break;
    case TYPE_STRING:
        break;
    case TYPE_UNIT:
        emit(g, "POPS GF@$t1");
        emit(g, "PUSHS string@()");
        break;
    case TYPE_BOOL: {
        int n = g->labels++;
        emit(g, "PUSHS bool@true");
        emit(g, "JUMPIFEQS $str%d$true", n);
        emit(g, "PUSHS string@false");
        emit(g, "JUMP $str%d$end", n);
        emit(g, "LABEL $str%d$true", n);
        emit(g, "PUSHS string@true");
        emit(g, "LABEL $str%d$end", n);
        break;
    }
    default:
        emit(g, "# TODO toString of this type");
        break;
    }
}

// built-in functions that need only a few stack instructions. returns false if name is not one of them.
static bool gen_simple_builtin(gen_t* g, const char* name, const tree_node_t* call)
{
    if (strcmp(name, "print") == 0) {
        push_args(g, call);
        emit(g, "POPS GF@$t1");
        if (first_arg_type(g, call) == TYPE_UNIT) {
            emit(g, "WRITE string@()");
        } else {
            emit(g, "WRITE GF@$t1");
        }
        emit(g, "PUSHS nil@nil");
    } else if (strcmp(name, "toStr") == 0) {
        push_args(g, call);
        emit_to_string(g, first_arg_type(g, call));
    } else if (strcmp(name, "toInt") == 0) {
        push_args(g, call);
        emit(g, "FLOAT2INTS");
    } else if (strcmp(name, "chr") == 0) {
        push_args(g, call);
        emit(g, "INT2CHARS");
    } else if (strcmp(name, "strLength") == 0) {
        push_args(g, call);
        emit(g, "POPS GF@$t1");
        emit(g, "STRLEN GF@$t3 GF@$t1");
        emit(g, "PUSHS GF@$t3");
    } else {
        return false;
    }
    return true;
}

// built-in functions written as helper functions
static bool is_helper_builtin(const char* name)
{
    return strcmp(name, "readLine") == 0 || strcmp(name, "readInt") == 0 || strcmp(name, "readDouble") == 0 || strcmp(name, "subStr") == 0 || strcmp(name, "ord") == 0
        || strcmp(name, "readBoolean") == 0 || strcmp(name, "readIntMaybe") == 0 || strcmp(name, "readDoubleMaybe") == 0;
}

// index of a field (PARAM of a case class) in its object;
// index 0 holds the class name, fields start at 1
static int field_index(gen_t* g, const tree_node_t* param)
{
    const tree_node_t* def = param;
    while (def && def->rule != RULE_CASE_CLASS_DEF) {
        def = def->parent;
    }
    node_list_t fields = { 0 };
    collect(g, def, RULE_PARAM, &fields);
    int idx = 0;
    for (size_t i = 0; i < fields.count; i++) {
        if (fields.items[i] == param) {
            idx = (int)i + 1;
        }
    }
    free(fields.items);
    return idx;
}

// creating array
static void gen_constructor(gen_t* g, const tree_node_t* call, const tree_node_t* def)
{
    push_args(g, call);
    node_list_t fields = { 0 };
    collect(g, def, RULE_PARAM, &fields);
    emit(g, "NEWARRAY GF@$t3 int@%zu", fields.count + 1);
    emit(g, "ARRAYSET GF@$t3 int@0 string@%s", def->children[2]->token.data);
    for (size_t i = fields.count; i > 0; i--) { // last argument on top
        emit(g, "POPS GF@$t2");
        emit(g, "ARRAYSET GF@$t3 int@%zu GF@$t2", i);
    }
    free(fields.items);
    emit(g, "PUSHS GF@$t3");
}

// function call: every call leaves exactly one value on the stack
static void gen_call(gen_t* g, const tree_node_t* call)
{
    const char* name = call->children[0]->token.data;

    // call of a case class = constructor
    const SemanticNodeInfo* i = semantic_info_find(g->info, call);
    if (i && i->declaration && i->declaration->rule == RULE_CASE_CLASS_DEF) {
        gen_constructor(g, call, i->declaration);
        return;
    }

    if (gen_simple_builtin(g, name, call)) {
        return;
    }
    push_args(g, call);
    emit(g, "CREATEFRAME");
    if (is_helper_builtin(name)) {
        emit(g, "CALL $b_%s", name);
    } else {
        emit(g, "CALL $f_%s", name);
    }
}

// -x as 0 - x, !x as NOT x
static void gen_unary(gen_t* g, const tree_node_t* node)
{
    const tree_node_t* operand = node->children[1];
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

// receiver.member (valmem): toString, toInt, floor, length
// member is EXPRESSION with one child: the member name
// node is the whole "receiver . member" expression
static void gen_member(gen_t* g, const tree_node_t* node)
{
    const tree_node_t* receiver = node->children[0];
    const tree_node_t* member = node->children[2];
    const tree_node_t* leaf = member->children[0];

    const SemanticNodeInfo* i = semantic_info_find(g->info, node);
    if (i && i->declaration && i->declaration->rule == RULE_FUNCTION_DEF) {
        gen_expr(g, receiver);
        if (leaf->type != NODE_TERMINAL) {
            push_args(g, leaf);
        }
        emit(g, "CREATEFRAME");
        emit(g, "CALL $m_%s$%zu", function_name(i->declaration), decl_id(g, i->declaration));
        return;
    }

    // field of a case class
    if (i && i->declaration && i->declaration->rule == RULE_PARAM) {
        gen_expr(g, receiver);
        emit(g, "POPS GF@$t1");
        emit(g, "ARRAYGET GF@$t2 GF@$t1 int@%d", field_index(g, i->declaration));
        emit(g, "PUSHS GF@$t2");
        return;
    }
    const char* name = leaf->token.data;
    gen_expr(g, receiver);

    if (strcmp(name, "toString") == 0) {
        emit_to_string(g, expr_type(g, receiver));
    } else if (strcmp(name, "toInt") == 0) {
        emit(g, "FLOAT2INTS"); // cuts off decimal part
    } else if (strcmp(name, "length") == 0) {
        emit(g, "POPS GF@$t1");
        emit(g, "STRLEN GF@$t3 GF@$t1");
        emit(g, "PUSHS GF@$t3");
    } else if (strcmp(name, "floor") == 0) {
        int n = g->labels++;
        emit(g, "POPS GF@$t1");
        emit(g, "FLOAT2INT GF@$t2 GF@$t1");
        emit(g, "INT2FLOAT GF@$t3 GF@$t2");
        emit(g, "PUSHS GF@$t3");
        emit(g, "PUSHS GF@$t1");
        emit(g, "GTS");
        emit(g, "PUSHS bool@true");
        emit(g, "JUMPIFNEQS $floor%d$ok", n);
        emit(g, "PUSHS GF@$t2");
        emit(g, "PUSHS int@1");
        emit(g, "SUBS");
        emit(g, "POPS GF@$t2");
        emit(g, "LABEL $floor%d$ok", n);
        emit(g, "PUSHS GF@$t2");
        emit(g, "INT2FLOATS");
    } else {
        g->error = 99;
    }
}

// a && b, a || b with short-ciruit evaluation: the right side is evaluated only if the left side does not decide
static void gen_logic(gen_t* g, const tree_node_t* left, const tree_node_t* right, bool is_and)
{
    int n = g->labels++;
    const char* stop = is_and ? "false" : "true";
    gen_expr(g, left);
    emit(g, "PUSHS bool@%s", stop);
    emit(g, "JUMPIFEQS $logic%d$short", n); // elft == stop -> result is stop
    gen_expr(g, right); // otherwise result = right
    emit(g, "JUMP $logic%d$end", n);
    emit(g, "LABEL $logic%d$short", n);
    emit(g, "PUSHS bool@%s", stop);
    emit(g, "LABEL $logic%d$end", n);
}

// E op E: both operands on the stack, then one stack instruction
static void gen_binary(gen_t* g, const tree_node_t* node)
{
    const tree_node_t* left = node->children[0];
    const tree_node_t* right = node->children[2];
    t_type op = node->children[1]->token.type;

    if (op == T_AND || op == T_OR) { // short-circuit evaluation
        gen_logic(g, left, right, op == T_AND);
        return;
    }
    if (op == T_DOT) { // member access
        gen_member(g, node);
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
static void gen_expr(gen_t* g, const tree_node_t* node)
{
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
    const SemanticNodeInfo* i = semantic_info_find(g->info, node);
    if (i && i->promote_to_double) {
        emit(g, "INT2FLOATS");
    }
}

static void gen_block(gen_t* g, const tree_node_t* block, bool keep);

// RHS: value
static void gen_rhs(gen_t* g, const tree_node_t* rhs)
{
    if (rhs->rule == RULE_RHS_2) {
        gen_value(g, rhs->children[0]);
    } else {
        gen_block(g, rhs->children[0], true);
    }
}

// STATEMENTS
static void gen_return(gen_t* g, const tree_node_t* ret)
{
    const tree_node_t* val = ret->children[1];
    if (val->rule == RULE_RETURN_VALUE_1) {
        gen_value(g, val->children[0]);
    } else {
        emit(g, "PUSHS nil@nil");
    }
    emit(g, "POPFRAME");
    emit(g, "RETURN");
}

static void gen_inline(gen_t* g, const tree_node_t* node, bool keep);

// BODY of if/while: block, one statement, or unary expression
static void gen_body(gen_t* g, const tree_node_t* body, bool keep)
{
    if (body->rule == RULE_BODY_1) {
        gen_block(g, body->children[0], keep);
    } else if (body->rule == RULE_BODY_2) {
        gen_inline(g, body->children[0], keep);
    } else {
        gen_expr(g, body->children[0]);
        if (!keep) {
            emit(g, "POPS GF@$t1");
        }
    }
}

// IF_STATEMENT children: if, EXPRESSION, then, BODY, ELSE_PART
static void gen_if(gen_t* g, const tree_node_t* node, bool keep)
{
    int n = g->labels++;
    gen_expr(g, node->children[1]);
    emit(g, "PUSHS bool@true");
    emit(g, "JUMPIFNEQS $if%d$else", n);
    gen_body(g, node->children[3], keep);
    emit(g, "JUMP $if%d$end", n);
    emit(g, "LABEL $if%d$else", n);
    const tree_node_t* else_part = node->children[4];
    if (else_part->rule == RULE_ELSE_PART_1) {
        gen_body(g, else_part->children[1], keep);
    } else if (keep) {
        emit(g, "PUSHS nil@nil");
    }
    emit(g, "LABEL $if%d$end", n);
}

// WHILE_STATEMENT children: whie, EXPRESSION, do, BODY
static void gen_while(gen_t* g, const tree_node_t* node, bool keep)
{
    int n = g->labels++;
    emit(g, "LABEL $while%d", n);
    gen_expr(g, node->children[1]);
    emit(g, "PUSHS bool@true");
    emit(g, "JUMPIFNEQS $while%d$end", n);
    gen_body(g, node->children[3], false);
    emit(g, "JUMP $while%d", n);
    emit(g, "LABEL $while%d$end", n);
    if (keep) {
        emit(g, "PUSHS nil@nil");
    }
}

// evaluates all guards of a for header, a false guard skips the body
static void gen_guards(gen_t* g, const tree_node_t* node, int n)
{
    if (node->type != NODE_NONTERMINAL) {
        return;
    }
    if (node->rule == RULE_INLINE_GUARDS_1 || node->rule == RULE_GUARD_LINES_1) {
        gen_expr(g, node->children[1]);
        emit(g, "PUSHS bool@true");
        emit(g, "JUMPIFNEQS $for%d$next", n);
    }
    for (size_t i = 0; i < node->children_count; i++) {
        gen_guards(g, node->children[i], n);
    }
}

// FOR_STATEMENT children: for, FOR_HEADER, do, BODY
// GENERATOR children: id, <-, EXPRESSION, RANGE_OP, EXPRESSION, STEP
static void gen_for(gen_t* g, const tree_node_t* node, bool keep)
{
    int n = g->labels++;
    const tree_node_t* header = node->children[1];
    const tree_node_t* gen = header->rule == RULE_FOR_HEADER_1 ? header->children[0] : header->children[2];
    const tree_node_t* step = gen->children[5];
    bool until = gen->children[3]->rule == RULE_RANGE_OP_2;
    const char* name = decl_name(gen);
    size_t id = decl_id(g, gen);

    // start, end and step are evaluated only once, before the loop
    gen_expr(g, gen->children[2]);
    emit(g, "POPS LF@%s$%zu", name, id);
    gen_expr(g, gen->children[4]);
    emit(g, "POPS LF@$end$%zu", id);
    if (step->rule == RULE_STEP_1) {
        gen_expr(g, step->children[1]);
        emit(g, "POPS LF@$step$%zu", id);
        emit(g, "JUMPIFNEQ $for%d$start LF@$step$%zu int@0", n, id);
        emit(g, "EXIT int@40");
    } else {
        emit(g, "MOVE LF@$step$%zu int@1", id);
    }

    // end test: step > 0 -> stop when i > end (until: i >= end)
    // step < 0 -> stop when i < end
    emit(g, "LABEL $for%d$start", n);
    emit(g, "GT GF@$t1 LF@$step$%zu int@0", id);
    emit(g, "JUMPIFEQ $for%d$down GF@$t1 bool@false", n);
    if (until) {
        emit(g, "LT GF@$t1 LF@%s$%zu LF@$end$%zu", name, id, id);
        emit(g, "JUMPIFEQ $for%d$end GF@$t1 bool@false", n);
    } else {
        emit(g, "GT GF@$t1 LF@%s$%zu LF@$end$%zu", name, id, id);
        emit(g, "JUMPIFEQ $for%d$end GF@$t1 bool@true", n);
    }
    emit(g, "JUMP $for%d$body", n);
    emit(g, "LABEL $for%d$down", n);
    emit(g, "LT GF@$t1 LF@%s$%zu LF@$end$%zu", name, id, id);
    emit(g, "JUMPIFEQ $for%d$end GF@$t1 bool@true", n);

    emit(g, "LABEL $for%d$body", n);
    gen_guards(g, header, n);
    gen_body(g, node->children[3], false);
    emit(g, "LABEL $for%d$next", n);
    emit(g, "ADD LF@%s$%zu LF@%s$%zu LF@$step$%zu", name, id, name, id, id);
    emit(g, "JUMP $for%d$start", n);
    emit(g, "LABEL $for%d$end", n);
    if (keep) {
        emit(g, "PUSHS nil@nil");
    }
}

// IFJcode26 type name (result of TYPE) for a type in a pattern; for a case class returns "array" and sets *cls to the class name
static const char* pattern_type(const tree_node_t* type, const char** cls)
{
    *cls = NULL;
    const tree_node_t* simple = type->children[0];
    switch (simple->rule) {
    case RULE_SIMPLE_TYPE_1:
        return "int";
    case RULE_SIMPLE_TYPE_2:
        return "float";
    case RULE_SIMPLE_TYPE_3:
        return "string";
    case RULE_SIMPLE_TYPE_4:
        return "nil";
    case RULE_SIMPLE_TYPE_5:
        return "bool";
    default:
        *cls = simple->children[0]->token.data;
        return "array";
    }
}

// jumps to fail unless var is an object of the case class cls
static void test_class(gen_t* g, const char* var, const char* cls, const char* fail)
{
    emit(g, "TYPE GF@$t1 %s", var);
    emit(g, "JUMPIFNEQ %s GF@$t1 string@array", fail);
    emit(g, "ARRAYGET GF@$t1 %s int@0", var);
    emit(g, "JUMPIFNEQ %s GF@$t1 string@%s", fail, cls);
}

// literal pattern: jumps to fail unless var equals it. the type is compared first, comparing different types is an error 53.
static void test_literal(gen_t* g, const tree_node_t* lit, const char* var, const char* fail)
{
    const char* type;
    switch (lit->rule) {
    case RULE_LITERAL_PATTERN_1:
        type = "int";
        break;
    case RULE_LITERAL_PATTERN_2:
        type = "float";
        break;
    case RULE_LITERAL_PATTERN_3:
        type = "string";
        break;
    case RULE_LITERAL_PATTERN_4:
        type = "bool";
        break;
    case RULE_LITERAL_PATTERN_5:
        type = lit->children[1]->rule == RULE_NUMBER_1 ? "int" : "float";
        break;
    default:
        type = "nil";
        break;
    }
    emit(g, "TYPE GF@$t1 %s", var);
    emit(g, "JUMPIFNEQ %s GF@$t1 string@%s", fail, type);
    emit(g, "PUSHS %s", var);
    if (lit->rule == RULE_LITERAL_PATTERN_5) {
        const tree_node_t* num = lit->children[1]->children[0];
        emit(g, num->token.type == T_INT ? "PUSHS int@0" : "PUSHS float@0x0p+0");
        gen_term(g, num);
        emit(g, "SUBS");
    } else {
        gen_term(g, lit->children[0]);
    }
    emit(g, "JUMPIFNEQS %s", fail);
}

// one pattern of a case: var holds the matched value
static void gen_pattern(gen_t* g, const tree_node_t* pat, const char* var, const char* fail)
{
    if (pat->rule == RULE_PATTERN_1) {
        return;
    }
    if (pat->rule == RULE_PATTERN_3) {
        test_literal(g, pat->children[0], var, fail);
        return;
    }
    const tree_node_t* detail = pat->children[1];
    if (detail->rule == RULE_ID_PATTERN_1) {
        const char* cls;
        const char* type = pattern_type(detail->children[1], &cls);
        if (cls) {
            test_class(g, var, cls, fail);
        } else {
            emit(g, "TYPE GF@$t1 %s", var);
            emit(g, "JUMPIFNEQ %s GF@$t1 string@%s", fail, type);
        }
        emit(g, "MOVE LF@%s$%zu %s", decl_name(pat), decl_id(g, pat), var);
        return;
    }
    test_class(g, var, pat->children[0]->token.data, fail);
    node_list_t order = { 0 };
    for (const tree_node_t* n = detail; n;) {
        const tree_node_t* next = NULL;
        for (size_t i = 0; i < n->children_count; i++) {
            const tree_node_t* c = n->children[i];
            if (c->rule == RULE_SUBPATTERN_1 || c->rule == RULE_SUBPATTERN_2 || c->rule == RULE_SUBPATTERN_3) {
                list_push(g, &order, c);
            } else if (c->rule == RULE_SUBPATTERN_LIST_1 || c->rule == RULE_SUBPATTERN_REST_1) {
                next = c;
            }
        }
        n = next;
    }
    for (size_t i = 0; i < order.count; i++) {
        const tree_node_t* sub = order.items[i];
        if (sub->rule == RULE_SUBPATTERN_1) { // bind field to a vriable
            emit(g, "ARRAYGET LF@%s$%zu %s int@%zu", decl_name(sub), decl_id(g, sub), var, i + 1);
        } else if (sub->rule == RULE_SUBPATTERN_3) {
            emit(g, "ARRAYGET GF@$t2 %s int@%zu", var, i + 1);
            test_literal(g, sub->children[0], "GF@$t2", fail);
        }
    }
    free(order.items);
}

static void gen_match(gen_t* g, const tree_node_t* node, bool keep)
{
    const tree_node_t* suffix = node->children[1];
    int n = g->labels++;
    char var[64];
    snprintf(var, sizeof(var), "LF@$match$%zu", decl_id(g, suffix));

    gen_expr(g, node->children[0]);
    emit(g, "POPS %s", var);

    int k = 0;
    for (const tree_node_t* list = suffix->children[3]; list->rule != RULE_CASE_LIST_REST_2; list = list->children[1], k++) {
        const tree_node_t* clause = list->children[0];
        char fail[64];
        snprintf(fail, sizeof(fail), "$match%d$case%d", n, k);

        gen_pattern(g, clause->children[1], var, fail);
        const tree_node_t* guard = clause->children[2];
        if (guard->rule == RULE_CASE_GUARD_1) {
            gen_expr(g, guard->children[1]);
            emit(g, "PUSHS bool@true");
            emit(g, "JUMPIFNEQS %s", fail);
        }
        gen_rhs(g, clause->children[4]);
        if (!keep) {
            emit(g, "POPS GF@$t1");
        }
        emit(g, "JUMP $match%d$end", n);
        emit(g, "LABEL %s", fail);
    }
    emit(g, "LABEL $match%d$end", n);
}

static void gen_inline(gen_t* g, const tree_node_t* node, bool keep)
{
    if (node->rule == RULE_INLINE_STATEMENT_1) {
        gen_return(g, node->children[0]);
        return;
    }
    if (node->rule == RULE_INLINE_STATEMENT_2) { // if
        gen_if(g, node->children[0], keep);
        return;
    }
    if (node->rule == RULE_INLINE_STATEMENT_3) { // while
        gen_while(g, node->children[0], keep);
        return;
    }
    if (node->rule == RULE_INLINE_STATEMENT_4) { // for
        gen_for(g, node->children[0], keep);
        return;
    }
    if (node->rule == RULE_INLINE_STATEMENT_5) {
        const tree_node_t* rest = node->children[node->children_count - 1];
        if (rest->rule == RULE_ID_STATEMENT_1) { // x = value
            gen_rhs(g, rest->children[1]);
            emit_var(g, "POPS", node->children[0]);
        } else if (rest->children[1]->rule == RULE_MATCH_SUFFIX_1) {
            gen_match(g, rest, keep);
            return;
        } else {
            gen_expr(g, rest->children[0]);
            if (!keep) {
                emit(g, "POPS GF@$t1");
            }
            return;
        }
    } else if (node->rule == RULE_INLINE_STATEMENT_6 && node->children[1]->rule == RULE_MATCH_SUFFIX_1) {
        gen_match(g, node, keep);
        return;
    } else if (node->rule == RULE_INLINE_STATEMENT_6) {
        gen_expr(g, node->children[0]);
        if (!keep) {
            emit(g, "POPS GF@$t1");
        }
        return;
    } else {
        emit(g, "# TODO statement");
    }
    if (keep) {
        emit(g, "PUSHS nil@nil");
    }
}

// STATEMENT_1 = val/var definition, STATEMENT_2 = other statements
static void gen_statement(gen_t* g, const tree_node_t* stmt, bool keep)
{
    if (stmt->rule == RULE_STATEMENT_1) {
        const tree_node_t* def = stmt->children[0]; // VAR_DEFINITION
        gen_rhs(g, def->children[4]);
        emit(g, "POPS LF@%s$%zu", decl_name(def), decl_id(g, def));
        if (keep) {
            emit(g, "PUSHS nil@nil");
        }
    } else {
        gen_inline(g, stmt->children[0], keep);
    }
}

// one line: statement
static void gen_statement_line(gen_t* g, const tree_node_t* line, bool keep)
{
    const tree_node_t* stmt = line->children[0];
    for (const tree_node_t* r = line->children[1]; r->rule == RULE_STATEMENT_LINE_REST_1; r = r->children[2]) {
        gen_statement(g, stmt, false);
        stmt = r->children[1];
    }
    gen_statement(g, stmt, keep);
}

// block children: \n, INDENT, STATEMENT_LIST, DEDENT
static void gen_block(gen_t* g, const tree_node_t* block, bool keep)
{
    const tree_node_t* list = block->children[2];
    const tree_node_t* line = list->children[0];
    for (const tree_node_t* r = list->children[1]; r->rule == RULE_STATEMENT_LIST_REST_1; r = r->children[1]) {
        gen_statement_line(g, line, false);
        line = r->children[0];
    }
    gen_statement_line(g, line, keep);
}

// FUNCTIONS
// returns the FUNCTION_DEF of a top-level item, or NULL if it is not a function
static const tree_node_t* item_function(const tree_node_t* item)
{
    switch (item->rule) {
    case RULE_TOP_ITEM_1:
        return item->children[2];
    case RULE_TOP_ITEM_2:
        return item->children[0];
    default:
        return NULL;
    }
}

// generates one function: label, its own frame, body, return
static void gen_function(gen_t* g, const tree_node_t* def)
{
    emit(g, "# --- function %s ---", function_name(def));

    // extension methods get a unique label
    const tree_node_t* ext = method_extension(def);
    if (ext) {
        emit(g, "LABEL $m_%s$%zu", function_name(def), decl_id(g, def));
    } else {
        emit(g, "LABEL $f_%s", function_name(def));
    }
    emit(g, "PUSHFRAME");

    node_list_t params = { 0 };
    collect(g, def->children[2], RULE_PARAM, &params);
    for (size_t i = 0; i < params.count; i++) {
        const tree_node_t* p = params.items[i];
        emit(g, "DEFVAR LF@%s$%zu", decl_name(p), decl_id(g, p));
    }
    for (size_t i = params.count; i > 0; i--) {
        const tree_node_t* p = params.items[i - 1];
        emit(g, "POPS LF@%s$%zu", decl_name(p), decl_id(g, p));
    }
    free(params.items);

    // method receiver (PARAM of the extension) was pushed first
    if (ext) {
        const tree_node_t* r = ext->children[2];
        emit(g, "DEFVAR LF@%s$%zu", decl_name(r), decl_id(g, r));
        emit(g, "POPS LF@%s$%zu", decl_name(r), decl_id(g, r));
    }

    // all local variables defined once, at the start of the function
    node_list_t locals = { 0 };
    collect(g, def->children[5], RULE_VAR_DEFINITION, &locals);
    for (size_t i = 0; i < locals.count; i++) {
        const tree_node_t* v = locals.items[i];
        emit(g, "DEFVAR LF@%s$%zu", decl_name(v), decl_id(g, v));
    }
    free(locals.items);

    // hidden variable if each match and pattern variables
    node_list_t matches = { 0 };
    collect(g, def->children[5], RULE_MATCH_SUFFIX_1, &matches);
    for (size_t i = 0; i < matches.count; i++) {
        emit(g, "DEFVAR LF@$match$%zu", decl_id(g, matches.items[i]));
    }
    free(matches.items);
    node_list_t binds = { 0 };
    collect(g, def->children[5], RULE_PATTERN_2, &binds);
    collect(g, def->children[5], RULE_SUBPATTERN_1, &binds);
    for (size_t i = 0; i < binds.count; i++) {
        const tree_node_t* b = binds.items[i];
        if (b->rule == RULE_PATTERN_2 && b->children[1]->rule != RULE_ID_PATTERN_1) {
            continue;
        }
        emit(g, "DEFVAR LF@%s$%zu", decl_name(b), decl_id(g, b));
    }
    free(binds.items);

    // for loop variables and their hidden end and step
    node_list_t loops = { 0 };
    collect(g, def->children[5], RULE_GENERATOR, &loops);
    for (size_t i = 0; i < loops.count; i++) {
        const tree_node_t* v = loops.items[i];
        size_t id = decl_id(g, v);
        emit(g, "DEFVAR LF@%s$%zu", decl_name(v), id);
        emit(g, "DEFVAR LF@$end$%zu", id);
        emit(g, "DEFVAR LF@$step$%zu", id);
    }
    free(loops.items);

    // body
    gen_rhs(g, def->children[5]);
    emit(g, "POPFRAME");
    emit(g, "RETURN");
}

// helper functions for built-ins that need jumps
static void gen_builtins(gen_t* g)
{
    // readLine(): READ returns nil at EOF -> ""
    emit(g, "# --- builtin readLine ---");
    emit(g, "LABEL $b_readLine");
    emit(g, "READ GF@$t1 string");
    emit(g, "JUMPIFNEQ $b_readLine$ok GF@$t1 nil@nil");
    emit(g, "MOVE GF@$t1 string@");
    emit(g, "LABEL $b_readLine$ok");
    emit(g, "PUSHS GF@$t1");
    emit(g, "RETURN");

    // readInt(), readDouble(): nil on the bad input -> 0
    emit(g, "# --- builtin readInt ---");
    emit(g, "LABEL $b_readInt");
    emit(g, "READ GF@$t1 int");
    emit(g, "JUMPIFNEQ $b_readInt$ok GF@$t1 nil@nil");
    emit(g, "MOVE GF@$t1 int@0");
    emit(g, "LABEL $b_readInt$ok");
    emit(g, "PUSHS GF@$t1");
    emit(g, "RETURN");

    emit(g, "# --- builtin readDouble ---");
    emit(g, "LABEL $b_readDouble");
    emit(g, "READ GF@$t1 float");
    emit(g, "JUMPIFNEQ $b_readDouble$ok GF@$t1 nil@nil");
    emit(g, "MOVE GF@$t1 float@0x0p+0");
    emit(g, "LABEL $b_readDouble$ok");
    emit(g, "PUSHS GF@$t1");
    emit(g, "RETURN");

    // readBoolean(): behaves like READ bool
    emit(g, "# --- builtin readBoolean ---");
    emit(g, "LABEL $b_readBoolean");
    emit(g, "READ GF@$t1 bool");
    emit(g, "PUSHS GF@$t1");
    emit(g, "RETURN");

    // readIntMaybe(), readDoubleMaybe(): nil on bad input is () (Unit)
    emit(g, "# --- builtin readIntMaybe ---");
    emit(g, "LABEL $b_readIntMaybe");
    emit(g, "READ GF@$t1 int");
    emit(g, "PUSHS GF@$t1");
    emit(g, "RETURN");
    emit(g, "# --- builtin readDoubleMaybe ---");
    emit(g, "LABEL $b_readDoubleMaybe");
    emit(g, "READ GF@$t1 float");
    emit(g, "PUSHS GF@$t1");
    emit(g, "RETURN");

    // ord(s, i): -1 if i is out of range
    emit(g, "# --- builtin ord ---");
    emit(g, "LABEL $b_ord");
    emit(g, "PUSHFRAME");
    emit(g, "DEFVAR LF@s");
    emit(g, "DEFVAR LF@i");
    emit(g, "DEFVAR LF@len");
    emit(g, "DEFVAR LF@cond");
    emit(g, "POPS LF@i");
    emit(g, "POPS LF@s");
    emit(g, "STRLEN LF@len LF@s");
    emit(g, "LT LF@cond LF@i int@0");
    emit(g, "JUMPIFEQ $b_ord$bad LF@cond bool@true");
    emit(g, "LT LF@cond LF@i LF@len");
    emit(g, "JUMPIFEQ $b_ord$bad LF@cond bool@false");
    emit(g, "PUSHS LF@s");
    emit(g, "PUSHS LF@i");
    emit(g, "STRI2INTS");
    emit(g, "POPFRAME");
    emit(g, "RETURN");
    emit(g, "LABEL $b_ord$bad");
    emit(g, "PUSHS int@-1");
    emit(g, "POPFRAME");
    emit(g, "RETURN");

    // subStr(s, start, end): error 40 for bad indexes
    emit(g, "# --- builtin subStr ---");
    emit(g, "LABEL $b_subStr");
    emit(g, "PUSHFRAME");
    emit(g, "DEFVAR LF@s");
    emit(g, "DEFVAR LF@a");
    emit(g, "DEFVAR LF@b");
    emit(g, "DEFVAR LF@len");
    emit(g, "DEFVAR LF@cond");
    emit(g, "DEFVAR LF@c");
    emit(g, "DEFVAR LF@res");
    emit(g, "DEFVAR LF@tmp");
    emit(g, "POPS LF@b");
    emit(g, "POPS LF@a");
    emit(g, "POPS LF@s");
    emit(g, "STRLEN LF@len LF@s");
    emit(g, "LT LF@cond LF@a int@0"); // start < 0
    emit(g, "JUMPIFEQ $b_subStr$err LF@cond bool@true");
    emit(g, "LT LF@cond LF@b int@0"); // end < 0
    emit(g, "JUMPIFEQ $b_subStr$err LF@cond bool@true");
    emit(g, "GT LF@cond LF@a LF@b"); // start > end
    emit(g, "JUMPIFEQ $b_subStr$err LF@cond bool@true");
    emit(g, "LT LF@cond LF@a LF@len"); // start >= length
    emit(g, "JUMPIFEQ $b_subStr$err LF@cond bool@false");
    emit(g, "GT LF@cond LF@b LF@len"); // end > length
    emit(g, "JUMPIFEQ $b_subStr$err LF@cond bool@true");
    emit(g, "MOVE LF@res string@");
    emit(g, "LABEL $b_subStr$loop");
    emit(g, "JUMPIFEQ $b_subStr$end LF@a LF@b");
    emit(g, "GETCHAR LF@c LF@s LF@a");
    emit(g, "CONCAT LF@tmp LF@res LF@c");
    emit(g, "MOVE LF@res LF@tmp");
    emit(g, "ADD LF@a LF@a int@1");
    emit(g, "JUMP $b_subStr$loop");
    emit(g, "LABEL $b_subStr$end");
    emit(g, "PUSHS LF@res");
    emit(g, "POPFRAME");
    emit(g, "RETURN");
    emit(g, "LABEL $b_subStr$err");
    emit(g, "EXIT int@40");
}

int generate_code(const tree_node_t* root, const SemanticInfo* info, FILE* out)
{
    gen_t g = { out, info, { 0 }, 0, 0 };

    const tree_node_t* top = tree_get_child(root, 2);

    // find the function marked with @main
    const tree_node_t* main_def = NULL;
    for (const tree_node_t* list = top; list && list->rule == RULE_TOP_LIST_1; list = list->children[1]) {
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
    for (const tree_node_t* list = top; list && list->rule == RULE_TOP_LIST_1; list = list->children[1]) {
        const tree_node_t* def = item_function(list->children[0]);
        if (def) {
            gen_function(&g, def);
        }
        if (list->children[0]->rule == RULE_TOP_ITEM_5) { // extension
            node_list_t methods = { 0 };
            collect(&g, list->children[0], RULE_FUNCTION_DEF, &methods);
            for (size_t i = 0; i < methods.count; i++) {
                gen_function(&g, methods.items[i]);
            }
            free(methods.items);
        }
    }
    gen_builtins(&g);
    free(g.decls.items);
    return g.error;
}