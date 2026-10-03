#define _POSIX_C_SOURCE 200809L
#include "scanner.h"

int line_now = 1;

void buffer_init(Buffer *buf){
    buf->length = 0;
    buf->capacity = 16;
    buf->data = malloc(buf->capacity*sizeof(char));
    if(buf->data){
        buf->data[buf->length] = '\0';
    }
}

void buffer_append(Buffer *buf, char c){
    if(buf->length >= buf->capacity-1){
        buf->capacity = buf->capacity*2;
        buf->data = realloc(buf->data,buf->capacity*sizeof(char));
    }
    buf->data[buf->length] = c;
    buf->length++;
    buf->data[buf->length] = '\0';
}

void buffer_clear(Buffer *buf){
    buf->length = 0;
}

void buffer_free(Buffer *buf){
    free(buf->data);
    buf->data = NULL;
    buf->length = 0;
    buf->capacity = 0;
}

bool is_keyword(Token *token){
    bool found = false;
    for(int i = 0; keywords[i].keyword != NULL; i++){
        if(strcmp(token->data,keywords[i].keyword) == 0){
            found = true;
            token->type = keywords[i].type;
            break;
        }
    }
    return found;
}

int get_token(FILE *src, Token *token){
    token->type = T_EOF;
    token->data = NULL;
    token->line = line_now;
    State state = S_START;
    Buffer buffer;
    buffer_init(&buffer);
    int c;

    while(1){
        c = fgetc(src);
        switch (state){
            case S_START:
                if(c == EOF){
                    token->type = T_EOF;
                    buffer_free(&buffer);
                    return 0;
                }
                if(c == 10){
                    line_now++;
                    token->type = T_LF;
                    token->line = line_now-1;
                    buffer_free(&buffer);
                    return 0;
                }
                if(isalpha(c) || c == '_'){
                    buffer_append(&buffer, (char)c);
                    state = S_ID;
                    break;
                }
                break;
            case S_ID:
                if(isalpha(c) || c == '_' || isdigit(c)){
                    buffer_append(&buffer, (char)c);
                }else{
                    ungetc(c, src);     //returning unused char back to entry
                    token->data = strdup(buffer.data);
                    token->type = T_ID;
                    is_keyword(token);
                    buffer_free(&buffer);
                    return 0;
                }
            break;
        }
    }
}
