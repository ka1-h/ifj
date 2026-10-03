#include <stdio.h>
#include <stdlib.h>
#include "scanner.h"

int main(int argc, char *argv[]){
    int result = 0;
    FILE *src, *dst;
    Token t;

    if (argc < 3) {
        perror("malo argumentov");
        return 1;
    }

    src = fopen(argv[1], "r");
    if(src == NULL){
        perror("subor1");
        return 1;
    }

    dst = fopen(argv[2], "w");
    if(dst == NULL){
        perror("subor2");
        fclose(src);
        return 1;
    }

    while(t.type != T_EOF){
        result = get_token(src, &t);
        if(result != 0){
            printf("nacitanie chyba");
            break;
        }
        fprintf(dst,"Token: type = %d, line = %d and data = %s\n", t.type, t.line, t.data ? t.data : "NULL");
        if (t.data != NULL){
            free(t.data);
            t.data = NULL;
        }
    }

    fclose(src);
    fclose(dst);
    return result;
}