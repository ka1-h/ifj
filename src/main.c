#include <stdio.h>
#include <stdlib.h>
#include "scanner.h"

int main(int argc, char *argv[]){
    int result = 0, dent = 0, pos = 0;
    int pole_dent[256] = {0};
    bool processed = true, mul_de = false;
    FILE *src = stdin;
    Token t;
    Buffer buff;

    if(argc > 1){
        src = fopen(argv[1], "r");
        if (src == NULL) {
            fprintf(stderr, "File opening error\n");
            return 60;
        }
    }

    buffer_init(&buff);
    while(t.type != T_EOF){
        result = get_token(src, &t, &buff, &dent, &processed, &pos, pole_dent, &mul_de);
        if(result == 1){
            fprintf(stderr, "Lexer stderr\n");
            if (t.data != NULL) {
                free(t.data);
                t.data = NULL;
            }
            break;
        }
        // FOR TESTING LEXER
        printf("Token: type = %d, line = %d and data = %s\n", t.type, t.line, t.data ? t.data : "NULL");
        if (t.data != NULL){
            free(t.data);
            t.data = NULL;
        }
    }

    buffer_free(&buff);
    if(src != stdin){
        fclose(src);
    }
    return result;
}