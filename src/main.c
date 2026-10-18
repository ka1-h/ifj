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

    tree_node_t *root = handle_tokens(&parser);
    tree_print(root, stderr);


    int result = parser.error;
    tree_dispose(&root);
    parser_free(&parser);
    if(src != stdin){
        fclose(src);
    }
    return result;
}
