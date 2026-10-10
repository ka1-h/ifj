#include <stdio.h>
#include <stdlib.h>
#include "scanner.h"
#include "syntactic.h"

int main(int argc, char *argv[]){
    FILE *src = stdin;
    Parser parser;

    if(argc > 1){
        src = fopen(argv[1], "r");
        if (src == NULL) {
            fprintf(stderr, "File opening error\n");
            return 60;
        }
    }

    // reads the first token; the parser then holds it as lookahead for SA
    if(!parser_init(&parser, src)){
        fprintf(stderr, "Lexer stderr\n");
    }

    // SA starts here

    handle_tokens(&parser);


    int result = parser.error;
    parser_free(&parser);
    if(src != stdin){
        fclose(src);
    }
    return result;
}
