#ifndef SCANNER_H
#define SCANNER_H
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>

typedef enum{
    T_ID,   //id

    KW_DEF, //key words
    KW_DO,
    KW_DOUBLE,
    KW_ELSE,
    KW_IF,
    KW_IMPORT,
    KW_INT,
    KW_RETURN,
    KW_STRING,
    KW_THEN,
    KW_UNIT,
    KW_VAL,
    KW_VAR,
    KW_WHILE,
    KW_BOOLEAN,
    KW_CASE,
    KW_CLASS,
    KW_EXTENSION,
    KW_FOR,
    KW_LIST, //pravdepodobne netreba spracovat v nasom pripade
    KW_MATCH,
    KW_TYPE,

    T_INT,      //int
    T_DOUBLE,   //double
    T_STRING,   //string

    T_UNIT_BRACKETS,// ()
    T_SPACE,        //biely znak
    T_LF,           //koniec riadku
    T_EOF,          //koniec suboru
    T_IFJ,          //ifj26.*
    T_MAIN,         //@main

    T_SEMIC,    //  ;
    T_COMMA,    //  ,
    T_LPARENTH, //  (
    T_RPARENTH, //  )
    T_EQU_SIGN, //  =
    T_PLUS,     //  +
    T_MINUS,    //  -
    T_MULT,     //  *
    T_DIV,      //  /
    T_LT,       //  <
    T_LOET,     //  <=
    T_GT,       //  >
    T_GOET,     //  >=
    T_ET,       //  ==
    T_NET,      //  !=

    //BOOLTHEN
    KW_TRUE,    //true
    KW_FALSE,   //false
    T_NEG,      // !
    T_AND,      // &&
    T_OR,       // ||

    //CONTROL
    T_ASSIG, // <-

    //VALMEM 
    T_DOT,   // .

    //ALGEBDT
    T_UNIF,             //  |
    T_COL,              //  :
    T_REPRESPATTERN,    //  _

    //INTERPOLATEDS
    T_INTERP,   //  $
    T_DOLLAR,   //  $$
    T_QUTMARKS, //  \"

    //LAMBDAS
    T_LAMB, //  =>

    T_INDENT, //61
    T_DEDENT, //62

    //CONTROL
    KW_TO,      //to
    KW_BY,    //by
    KW_UNTIL,   //until
}t_type;

typedef struct{
    t_type type;
    char *data;
    int line;
}Token;

typedef struct{
    const char *keyword;
    t_type type;
}KeyWord;

static const KeyWord keywords[] = {
    {"def",KW_DEF},
    {"do",KW_DO},
    {"Double",KW_DOUBLE},
    {"else",KW_ELSE},
    {"if",KW_IF},
    {"import",KW_IMPORT},
    {"Int",KW_INT},
    {"return",KW_RETURN},
    {"String",KW_STRING},
    {"then",KW_THEN},
    {"Unit",KW_UNIT},
    {"val",KW_VAL},
    {"var",KW_VAR},
    {"while",KW_WHILE},
    {"Boolean",KW_BOOLEAN},
    {"true",KW_TRUE},
    {"false",KW_FALSE},
    {"case",KW_CASE},
    {"class",KW_CLASS},
    {"extension",KW_EXTENSION},
    {"for",KW_FOR},
    {"List",KW_LIST},
    {"match",KW_MATCH},
    {"type",KW_TYPE},
    {"to",KW_TO},
    {"by",KW_BY},
    {"until",KW_UNTIL},
    {NULL, T_ID}
};

typedef struct{
    char *data;
    size_t length;  //better instead of int for owerflow
    size_t capacity; 
}Buffer;

typedef enum{
    S_START,
    S_ID,
    S_REPRESPATTERN,
    S_IFJ,
    S_ZERO,
    S_ZERO_UNDER,
    S_INT,
    S_INT_UNDER,
    S_PLUS,
    S_MINUS,
    S_DOT,
    S_EXP,
    S_EXP_SIGN,
    S_DOUBLE,
    S_DOUBLE_UNDER,
    S_STR1,
    S_ESC_SEQ,
    S_STR,
    S_STR_MUL,
    S_STR_MUL1,
    S_STR_MUL2,
    S_MAIN1,
    S_MAIN2,
    S_MAIN3,
    S_MAIN,
    S_DIV_COMMENT,
    S_ROW_COM,
    S_BLOCK_COM1,
    S_BLOCK_COM2,
    S_BLOCK_NEST_B,
    S_BLOCK_NEST_E,
    S_BLOCK_WAIT_END,
    S_LPARENTH,
    S_EQU_SIGN,
    S_LT,
    S_GT,
    S_NEG,
    S_AND,
    S_UNIF,
    S_INTERP,
    S_QUTMARKS,
    S_LF,
}State;

void buffer_init(Buffer *buf);
void buffer_append(Buffer *buf, char c);
void buffer_clear(Buffer *buf);
void buffer_free(Buffer *buf);
bool is_keyword(Token *token);
int get_token(FILE *src, Token *token, Buffer *buffer, int *dent, bool *processed, int *pos, int pole_dent[], bool *mul_de);
int what_dent(int dent, int *pos, int pole_dent[]);

#endif