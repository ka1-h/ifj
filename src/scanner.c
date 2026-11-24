#define _POSIX_C_SOURCE 200809L
#include "scanner.h"

int line_now = 1;
bool gap = false, next_lf = false, found_com_lf = false;

void buffer_init(Buffer* buf)
{
    buf->length = 0;
    buf->capacity = 16;
    buf->data = malloc(buf->capacity * sizeof(char));
    if (buf->data) {
        buf->data[buf->length] = '\0';
    }
}

void buffer_append(Buffer* buf, char c)
{
    if (buf->length >= buf->capacity - 1) {
        buf->capacity = buf->capacity * 2;
        buf->data = realloc(buf->data, buf->capacity * sizeof(char));
    }
    buf->data[buf->length] = c;
    buf->length++;
    buf->data[buf->length] = '\0';
}

void buffer_clear(Buffer* buf)
{
    if (buf->data != NULL) {
        buf->data[0] = '\0';
    }
    buf->length = 0;
}

void buffer_free(Buffer* buf)
{
    free(buf->data);
    buf->data = NULL;
    buf->length = 0;
    buf->capacity = 0;
}

bool is_keyword(Token* token)
{
    bool found = false;
    for (int i = 0; keywords[i].keyword != NULL; i++) {
        if (strcmp(token->data, keywords[i].keyword) == 0) {
            found = true;
            token->type = keywords[i].type;
            break;
        }
    }
    return found;
}

int what_dent(int dent, int* pos, int pole_dent[])
{
    if (dent > pole_dent[*pos]) {
        (*pos)++;
        pole_dent[*pos] = dent;
        return 1;
    } else if (dent < pole_dent[*pos]) {
        if (*pos <= 0) {
            return -2;
        }
        (*pos)--;
        return -1;
    } else {
        return 0;
    }
}

int get_token(FILE* src, Token* token, Buffer* buffer, int* dent, bool* processed, int* pos, int pole_dent[], bool* mul_de)
{
    token->type = T_EOF;
    token->data = NULL;
    token->line = line_now;
    State state = S_START;
    int c = -1, left = 0, right = 0, res = 0;
    bool exponent = false, has_d = false, has_end = false;
    if (!(*processed)) {
        res = what_dent(*dent, pos, pole_dent);
        if (res == 1) {
            *mul_de = true;
            *dent = 0;
            *processed = true;
            token->type = T_INDENT;
            return 0;
        } else if (res == -1) {
            if (pole_dent[*pos] == 0) {
                *mul_de = false;
            }
            token->type = T_DEDENT;
            return 0;
        } else if (res == 0) {
            *dent = 0;
            *processed = true;
        } else {
            return 1;
        }
    }

    while (1) {
        c = fgetc(src);
        switch (state) {
        case S_START:
            token->line = line_now;
            if (c == EOF) {
                if (pole_dent[*pos] != 0) {
                    what_dent(*dent, pos, pole_dent);
                    ungetc(c, src);
                    token->type = T_DEDENT;
                    buffer_clear(buffer);
                    return 0;
                }
                token->type = T_EOF;
                buffer_clear(buffer);
                return 0;
            }
            // LF
            if (c == 10) {
                line_now++;
                buffer_append(buffer, (char)c);
                state = S_LF;
                break;
            }
            // TAB
            if (c == 9) {
                buffer_clear(buffer);
                return 1;
            }
            if (c == ' ') {
                token->type = T_SPACE;
                buffer_clear(buffer);
                return 0;
            }
            if (isalpha(c)) {
                buffer_append(buffer, (char)c);
                state = S_ID;
                break;
            }
            if (c == '_') {
                buffer_append(buffer, (char)c);
                state = S_REPRESPATTERN;
                break;
            }
            if (isdigit(c)) {
                if (c == '0') {
                    buffer_append(buffer, (char)c);
                    state = S_ZERO;
                    break;
                } else {
                    buffer_append(buffer, (char)c);
                    state = S_INT;
                    break;
                }
            }
            if (c == '-') {
                buffer_append(buffer, (char)c);
                state = S_MINUS;
                break;
            }
            if (c == '+') {
                buffer_append(buffer, (char)c);
                state = S_PLUS;
                break;
            }
            if (c == '.') {
                buffer_append(buffer, (char)c);
                state = S_DOT;
                break;
            }
            if (c == '"') {
                buffer_append(buffer, (char)c);
                state = S_STR1;
                break;
            }
            if (c == '@') {
                buffer_append(buffer, (char)c);
                state = S_MAIN1;
                break;
            }
            buffer_clear(buffer);
            if (c == '/') {
                buffer_append(buffer, (char)c);
                state = S_DIV_COMMENT;
                break;
            }
            if (c == ';') {
                buffer_append(buffer, (char)c);
                token->data = strdup(buffer->data);
                token->type = T_SEMIC;
                buffer_clear(buffer);
                return 0;
            }
            if (c == ',') {
                buffer_append(buffer, (char)c);
                token->data = strdup(buffer->data);
                token->type = T_COMMA;
                buffer_clear(buffer);
                return 0;
            }
            if (c == '(') {
                buffer_append(buffer, (char)c);
                state = S_LPARENTH;
                break;
            }
            if (c == ')') {
                buffer_append(buffer, (char)c);
                token->data = strdup(buffer->data);
                token->type = T_RPARENTH;
                buffer_clear(buffer);
                return 0;
            }
            if (c == '=') {
                buffer_append(buffer, (char)c);
                state = S_EQU_SIGN;
                break;
            }
            if (c == '*') {
                buffer_append(buffer, (char)c);
                token->data = strdup(buffer->data);
                token->type = T_MULT;
                buffer_clear(buffer);
                return 0;
            }
            if (c == '<') {
                buffer_append(buffer, (char)c);
                state = S_LT;
                break;
            }
            if (c == '>') {
                buffer_append(buffer, (char)c);
                state = S_GT;
                break;
            }
            if (c == '!') {
                buffer_append(buffer, (char)c);
                state = S_NEG;
                break;
            }
            if (c == '&') {
                buffer_append(buffer, (char)c);
                state = S_AND;
                break;
            }
            if (c == '|') {
                buffer_append(buffer, (char)c);
                state = S_UNIF;
                break;
            }
            if (c == ':') {
                buffer_append(buffer, (char)c);
                token->data = strdup(buffer->data);
                token->type = T_COL;
                buffer_clear(buffer);
                return 0;
            }
            if (c == '$') {
                buffer_append(buffer, (char)c);
                state = S_INTERP;
                break;
            }
            if (c == '\\') {
                buffer_append(buffer, (char)c);
                state = S_QUTMARKS;
                break;
            }
            buffer_clear(buffer);
            return 1;
        case S_ID:
            if (isalpha(c) || c == '_' || isdigit(c)) {
                buffer_append(buffer, (char)c);
                break;
            } else if (c == '.' && strcmp(buffer->data, "ifj26") == 0) {
                buffer_append(buffer, (char)c);
                state = S_IFJ;
                break;
            } else {
                ungetc(c, src);
                token->data = strdup(buffer->data);
                token->type = T_ID;
                is_keyword(token);
                buffer_clear(buffer);
                return 0;
            }
        case S_REPRESPATTERN:
            if (isalpha(c) || c == '_' || isdigit(c)) {
                buffer_append(buffer, (char)c);
                state = S_ID;
                break;
            } else {
                ungetc(c, src);
                token->data = strdup(buffer->data);
                token->type = T_REPRESPATTERN;
                buffer_clear(buffer);
                return 0;
            }
        case S_IFJ:
            if (c == '*') {
                buffer_append(buffer, (char)c);
                token->data = strdup(buffer->data);
                token->type = T_IFJ;
                buffer_clear(buffer);
                return 0;
            } else {
                buffer_clear(buffer);
                return 2;
            }
        case S_ZERO:
            if (isdigit(c)) {
                buffer_clear(buffer);
                return 1;
            } else if (c == '_') {
                state = S_ZERO_UNDER;
                break;
            } else if (c == '.') {
                buffer_append(buffer, (char)c);
                state = S_DOUBLE;
                break;
            } else if (c == 'e' || c == 'E') {
                buffer_append(buffer, (char)c);
                state = S_EXP;
                break;
            } else {
                ungetc(c, src);
                token->data = strdup(buffer->data);
                token->type = T_INT;
                buffer_clear(buffer);
                return 0;
            }
        case S_ZERO_UNDER:
            if (isdigit(c)) {
                state = S_ZERO;
                break;
            } else {
                buffer_clear(buffer);
                return 1;
            }
        case S_INT:
            if (isdigit(c)) {
                buffer_append(buffer, (char)c);
                break;
            } else if (c == '_') {
                buffer_append(buffer, (char)c);
                state = S_INT_UNDER;
                break;
            } else if (c == '.') {
                buffer_append(buffer, (char)c);
                state = S_DOT;
                break;
            } else if (c == 'e' || c == 'E') {
                buffer_append(buffer, (char)c);
                state = S_EXP;
                break;
            } else {
                ungetc(c, src);
                token->data = strdup(buffer->data);
                token->type = T_INT;
                buffer_clear(buffer);
                return 0;
            }
        case S_INT_UNDER:
            if (isdigit(c)) {
                buffer_append(buffer, (char)c);
                state = S_INT;
                break;
            } else {
                buffer_clear(buffer);
                return 1;
            }
        case S_MINUS:
            ungetc(c, src);
            token->data = strdup(buffer->data);
            token->type = T_MINUS;
            buffer_clear(buffer);
            return 0;
        case S_PLUS:
            ungetc(c, src);
            token->data = strdup(buffer->data);
            token->type = T_PLUS;
            buffer_clear(buffer);
            return 0;
        case S_DOT:
            if (isdigit(c)) {
                buffer_append(buffer, (char)c);
                state = S_DOUBLE;
                break;
            } else {
                ungetc(c, src);
                if (strlen(buffer->data) > 1) {
                    buffer_clear(buffer);
                    return 1;
                } else {
                    token->data = strdup(buffer->data);
                    token->type = T_DOT;
                    buffer_clear(buffer);
                    return 0;
                }
            }
        case S_EXP:
            if (isdigit(c)) {
                exponent = true;
                buffer_append(buffer, (char)c);
                state = S_DOUBLE;
                break;
            } else if (c == '+' || c == '-') {
                buffer_append(buffer, (char)c);
                state = S_EXP_SIGN;
                break;
            } else {
                buffer_clear(buffer);
                return 1;
            }
        case S_EXP_SIGN:
            if (isdigit(c)) {
                exponent = true;
                buffer_append(buffer, (char)c);
                state = S_DOUBLE;
                break;
            } else {
                buffer_clear(buffer);
                return 1;
            }
        case S_DOUBLE:
            if (isdigit(c)) {
                buffer_append(buffer, (char)c);
                break;
            } else if (c == 'e' || c == 'E') {
                if (!exponent) {
                    buffer_append(buffer, (char)c);
                    state = S_EXP;
                    break;
                } else {
                    buffer_clear(buffer);
                    return 1;
                }
            } else if (c == 'd' || c == 'D') {
                if (!has_d) {
                    has_d = true;
                    buffer_append(buffer, (char)c);
                    break;
                } else {
                    buffer_clear(buffer);
                    return 1;
                }
            } else if (c == '_') {
                buffer_append(buffer, (char)c);
                state = S_DOUBLE_UNDER;
                break;
            } else {
                ungetc(c, src);
                token->data = strdup(buffer->data);
                token->type = T_DOUBLE;
                buffer_clear(buffer);
                return 0;
            }
        case S_DOUBLE_UNDER:
            if (isdigit(c)) {
                buffer_append(buffer, (char)c);
                state = S_DOUBLE;
                break;
            } else {
                buffer_clear(buffer);
                return 1;
            }
        case S_STR1:
            if (c == '"') {
                buffer_append(buffer, (char)c);
                state = S_STR;
                break;
            } else if (c > 31 && (c != 34 && c != 92)) {
                buffer_append(buffer, (char)c);
                break;
            } else if (c == '\\') {
                buffer_append(buffer, '\\');
                state = S_ESC_SEQ;
                break;
            } else {
                buffer_clear(buffer);
                return 1;
            }
        case S_ESC_SEQ:
            if (c == '\\') {
                buffer_append(buffer, '\\');
            } else if (c == '"') {
                buffer_append(buffer, '"');
            } else if (c == 'n') {
                buffer_append(buffer, 'n');
            } else if (c == 'r') {
                buffer_append(buffer, 'r');
            } else if (c == 't') {
                buffer_append(buffer, 't');
            } else {
                buffer_clear(buffer);
                return 1;
            }
            state = S_STR1;
            break;
        case S_STR:
            if (c == '"' && strcmp(buffer->data, "\"\"") == 0) {
                buffer_append(buffer, (char)c);
                state = S_STR_MUL;
                break;
            } else {
                ungetc(c, src);
                token->data = strdup(buffer->data);
                token->type = T_STRING;
                buffer_clear(buffer);
                return 0;
            }
        case S_STR_MUL:
            if (c != 34) {
                if (c == 10) {
                    line_now++;
                }
                buffer_append(buffer, (char)c);
                break;
            } else if (c == 34) {
                buffer_append(buffer, (char)c);
                state = S_STR_MUL1;
                break;
            } else {
                buffer_clear(buffer);
                return 1;
            }
        case S_STR_MUL1:
            if (c == 34) {
                buffer_append(buffer, (char)c);
                state = S_STR_MUL2;
                break;
            } else {
                buffer_clear(buffer);
                return 1;
            }
        case S_STR_MUL2:
            if (c == 34) {
                buffer_append(buffer, (char)c);
                state = S_STR;
                break;
            } else {
                buffer_clear(buffer);
                return 1;
            }
        case S_MAIN1:
            if (c == 'm') {
                buffer_append(buffer, (char)c);
                state = S_MAIN2;
                break;
            } else {
                buffer_clear(buffer);
                return 1;
            }
        case S_MAIN2:
            if (c == 'a') {
                buffer_append(buffer, (char)c);
                state = S_MAIN3;
                break;
            } else {
                buffer_clear(buffer);
                return 1;
            }
        case S_MAIN3:
            if (c == 'i') {
                buffer_append(buffer, (char)c);
                state = S_MAIN;
                break;
            } else {
                buffer_clear(buffer);
                return 1;
            }
        case S_MAIN:
            if (c == 'n') {
                buffer_append(buffer, (char)c);
                token->data = strdup(buffer->data);
                token->type = T_MAIN;
                buffer_clear(buffer);
                return 0;
            } else {
                buffer_clear(buffer);
                return 1;
            }
        case S_DIV_COMMENT:
            if (c == '/') {
                buffer_append(buffer, (char)c);
                state = S_ROW_COM;
                break;
            } else if (c == '*') {
                buffer_append(buffer, (char)c);
                state = S_BLOCK_COM1;
                break;
            } else {
                ungetc(c, src);
                token->data = strdup(buffer->data);
                token->type = T_DIV;
                buffer_clear(buffer);
                return 0;
            }
        case S_ROW_COM:
            if (c == 10 || c == EOF) {
                ungetc(c, src);
                buffer_clear(buffer);
                state = S_START;
                break;
            } else {
                buffer_append(buffer, (char)c);
                break;
            }
        case S_BLOCK_COM1:
            if (c == 10) {
                has_end = true;
                line_now++;
                buffer_append(buffer, (char)c);
                break;
            } else if (c == EOF) {
                buffer_clear(buffer);
                return 1;
            } else if (c == '/') {
                buffer_append(buffer, (char)c);
                state = S_BLOCK_NEST_B;
                break;
            } else if (c == '*') {
                if (left != right) {
                    buffer_append(buffer, (char)c);
                    state = S_BLOCK_NEST_E;
                    break;
                } else {
                    buffer_append(buffer, (char)c);
                    state = S_BLOCK_COM2;
                    break;
                }
            } else {
                buffer_append(buffer, (char)c);
                break;
            }
        case S_BLOCK_NEST_B:
            if (c == 10) {
                has_end = true;
                line_now++;
                buffer_append(buffer, (char)c);
                state = S_BLOCK_COM1;
                break;
            } else if (c == EOF) {
                buffer_clear(buffer);
                return 1;
            } else if (c == '*') {
                left++;
                buffer_append(buffer, (char)c);
                state = S_BLOCK_COM1;
                break;
            } else {
                buffer_append(buffer, (char)c);
                state = S_BLOCK_COM1;
                break;
            }
        case S_BLOCK_NEST_E:
            if (c == 10) {
                has_end = true;
                line_now++;
                buffer_append(buffer, (char)c);
                state = S_BLOCK_COM1;
                break;
            } else if (c == EOF) {
                buffer_clear(buffer);
                return 1;
            } else if (c == '/') {
                right++;
                buffer_append(buffer, (char)c);
                state = S_BLOCK_COM1;
                break;
            } else {
                buffer_append(buffer, (char)c);
                state = S_BLOCK_COM1;
                break;
            }
        case S_BLOCK_COM2:
            if (c == 10) {
                has_end = true;
                line_now++;
                buffer_append(buffer, (char)c);
                state = S_BLOCK_COM1;
                break;
            } else if (c == EOF) {
                buffer_clear(buffer);
                return 1;
            } else if (c == '/') {
                if (has_end) {
                    buffer_append(buffer, (char)c);
                    state = S_BLOCK_WAIT_END;
                    break;
                } else {
                    buffer_append(buffer, (char)c);
                    buffer_clear(buffer);
                    token->type = T_SPACE;
                    return 0;
                }
            } else {
                buffer_append(buffer, (char)c);
                state = S_BLOCK_COM1;
                break;
            }
        case S_BLOCK_WAIT_END:
            if (c == ' ') {
                buffer_append(buffer, (char)c);
                break;
            } else if (c == 10 || c == EOF) {
                ungetc(c, src);
                buffer_clear(buffer);
                state = S_START;
                break;
            } else {
                buffer_clear(buffer);
                return 1;
            }
        case S_LPARENTH:
            if (c == ')') {
                buffer_append(buffer, (char)c);
                token->data = strdup(buffer->data);
                token->type = T_UNIT_BRACKETS;
                buffer_clear(buffer);
                return 0;
            } else {
                ungetc(c, src);
                token->data = strdup(buffer->data);
                token->type = T_LPARENTH;
                buffer_clear(buffer);
                return 0;
            }
        case S_EQU_SIGN:
            if (c == '=') {
                buffer_append(buffer, (char)c);
                token->data = strdup(buffer->data);
                token->type = T_ET;
                buffer_clear(buffer);
                return 0;
            } else if (c == '>') {
                buffer_append(buffer, (char)c);
                token->data = strdup(buffer->data);
                token->type = T_LAMB;
                buffer_clear(buffer);
                return 0;
            } else {
                ungetc(c, src);
                token->data = strdup(buffer->data);
                token->type = T_EQU_SIGN;
                buffer_clear(buffer);
                return 0;
            }
        case S_LT:
            if (c == '=') {
                buffer_append(buffer, (char)c);
                token->data = strdup(buffer->data);
                token->type = T_LOET;
                buffer_clear(buffer);
                return 0;
            } else if (c == '-') {
                buffer_append(buffer, (char)c);
                token->data = strdup(buffer->data);
                token->type = T_ASSIG;
                buffer_clear(buffer);
                return 0;
            } else {
                ungetc(c, src);
                token->data = strdup(buffer->data);
                token->type = T_LT;
                buffer_clear(buffer);
                return 0;
            }
        case S_GT:
            if (c == '=') {
                buffer_append(buffer, (char)c);
                token->data = strdup(buffer->data);
                token->type = T_GOET;
                buffer_clear(buffer);
                return 0;
            } else {
                ungetc(c, src);
                token->data = strdup(buffer->data);
                token->type = T_GT;
                buffer_clear(buffer);
                return 0;
            }
        case S_NEG:
            if (c == '=') {
                buffer_append(buffer, (char)c);
                token->data = strdup(buffer->data);
                token->type = T_NET;
                buffer_clear(buffer);
                return 0;
            } else {
                ungetc(c, src);
                token->data = strdup(buffer->data);
                token->type = T_NEG;
                buffer_clear(buffer);
                return 0;
            }
        case S_AND:
            if (c == '&') {
                buffer_append(buffer, (char)c);
                token->data = strdup(buffer->data);
                token->type = T_AND;
                buffer_clear(buffer);
                return 0;
            } else {
                buffer_clear(buffer);
                return 1;
            }
        case S_UNIF:
            if (c == '|') {
                buffer_append(buffer, (char)c);
                token->data = strdup(buffer->data);
                token->type = T_OR;
                buffer_clear(buffer);
                return 0;
            } else {
                ungetc(c, src);
                token->data = strdup(buffer->data);
                token->type = T_UNIF;
                buffer_clear(buffer);
                return 0;
            }
        case S_INTERP:
            if (c == '$') {
                buffer_append(buffer, (char)c);
                token->data = strdup(buffer->data);
                token->type = T_DOLLAR;
                buffer_clear(buffer);
                return 0;
            } else {
                ungetc(c, src);
                token->data = strdup(buffer->data);
                token->type = T_INTERP;
                buffer_clear(buffer);
                return 0;
            }
        case S_QUTMARKS:
            if (c == '"') {
                buffer_append(buffer, (char)c);
                token->data = strdup(buffer->data);
                token->type = T_QUTMARKS;
                buffer_clear(buffer);
                return 0;
            } else {
                buffer_clear(buffer);
                return 1;
            }
        case S_LF:
            if (c == 10) {
                line_now++;
                *dent = 0;
                break;
            } else if (c == 9) {
                buffer_clear(buffer);
                return 1;
            } else if (c == ' ') {
                (*dent)++;
                break;
            } else if (c == '/') {
                *dent = 0;
                ungetc(c, src);
                token->type = T_LF;
                buffer_clear(buffer);
                state = S_START;
                return 0;
            } else {
                ungetc(c, src);
                if (*dent != 0 || *mul_de == true) {
                    *processed = false;
                    token->type = T_LF;
                    buffer_clear(buffer);
                    return 0;
                } else {
                    token->type = T_LF;
                    buffer_clear(buffer);
                    return 0;
                }
            }
            break;
        }
    }
}