#include <stdio.h>
#include <stdlib.h>
#include "scanner.h"

int main(int argc, char *argv[]){
    int result = 0;
    FILE *src, *dst;
    Token t;
    Buffer buff;

    if (argc < 3) {
        perror("malo argumentov\n");
        return 1;
    }

    src = fopen(argv[1], "r");
    if(src == NULL){
        perror("subor1\n");
        return 1;
    }

    dst = fopen(argv[2], "w");
    if(dst == NULL){
        perror("subor2\n");
        fclose(src);
        return 1;
    }

    buffer_init(&buff);
    while(t.type != T_EOF){
        result = get_token(src, &t, &buff);
        if(result == 1){
            fprintf(stderr, "Lexer stderr\n");
            break;
        }
        if(result != 1 && result != 0){
            printf("v lexe mas zly vypis\n");
            break;
        }
        fprintf(dst,"Token: type = %d, line = %d and data = %s\n", t.type, t.line, t.data ? t.data : "NULL");
        if (t.data != NULL){
            free(t.data);
            t.data = NULL;
        }
    }

    buffer_free(&buff);
    fclose(src);
    fclose(dst);
    return result;
}