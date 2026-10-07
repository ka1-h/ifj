#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdbool.h>
#include <stddef.h>

#define TABLE_SIZE 97

// Datove typy IFJ26 
typedef enum {
    TYPE_UNDEF,     
    TYPE_INT,
    TYPE_DOUBLE,
    TYPE_STRING,
    TYPE_UNIT,
    TYPE_BOOL       
} DataType;

// Stav polozky v tabulke
typedef enum {
    SLOT_EMPTY,
    SLOT_OCCUPIED
} State;

typedef enum {
    SYM_VAR,
    SYM_FUNC
} IdentifType;

typedef struct {
    DataType type;
    bool is_mutable;    // true = var, false = val (a parametre funkcie) 
} VarData;

typedef struct {
    DataType *param_types;  
    size_t param_count;
    DataType return_type; // NULL ak nieje znamy
} FuncData;

// Jedna polozka tabulky 
typedef struct {
    char *key;
    State state;
    IdentifType type;
    VarData var;
    FuncData func;
} Symbol;

typedef struct {
    Symbol *entries;     
    size_t size;         
    size_t count;        
} SymTable;

unsigned long hash_djb2(const char *str);

bool symtable_init(SymTable *t);
void symtable_free(SymTable *t);
Symbol *symtable_insert(SymTable *t, const char *key);   // NULL ak uz existuje alebo chyba alokacie 
Symbol *symtable_lookup(const SymTable *t, const char *key);

#endif 