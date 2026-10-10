#include <stdlib.h>
#include <stdio.h>

#include "syntactic.h"
#include "tree.h"

// Reads the next token that is not whitespace into p->current
static bool fetch_token(Parser *p) {
    do {
        int res = get_token(p->src, &p->current, &p->buffer, &p->dent, &p->processed,
                            &p->pos, p->pole_dent, &p->mul_de);
        if (res != 0) {
            free(p->current.data);
            p->current.data = NULL;
            if (p->error == PARSE_OK) {
                p->error = PARSE_ERR_LEX;
            }
            return false;
        }
        if (p->current.type == T_SPACE) {
            free(p->current.data);
            p->current.data = NULL;
        }
    } while (p->current.type == T_SPACE);
    return true;
}

bool parser_init(Parser *p, FILE *src) {
    p->src = src;
    p->dent = 0;
    p->pos = 0;
    for (int i = 0; i < 256; i++) {
        p->pole_dent[i] = 0;
    }
    p->processed = true;
    p->mul_de = false;
    p->current.type = T_EOF;
    p->current.data = NULL;
    p->current.line = 0;
    p->error = PARSE_OK;
    buffer_init(&p->buffer);

    return fetch_token(p);
}

void parser_free(Parser *p) {
    free(p->current.data);
    p->current.data = NULL;
    buffer_free(&p->buffer);
}

