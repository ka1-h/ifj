
#include "semantics.h"
#include "symtable.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>



//najde ulozene info k uzlu
const SemanticNodeInfo *semantic_info_find(const SemanticInfo *info, const tree_node_t *node) {
    
    for(size_t idx = 0; idx < info->count; idx++) {
        
        if(info->items[idx].node == node) {
            return &info->items[idx];

        }
    }

    return NULL;
}


//uvolni pole s info o uzlech
void semantic_info_free(SemanticInfo *info) {
   
    for(size_t idx = 0; idx < info->count; idx++) {
       
        free(info->items[idx].full_type.members);
    }

    free(info->items);
    info->items = NULL;
    info->count = 0;
    info->capacity = 0;
}



//vrati zaznam uzlu (nebo udela novy)
static SemanticNodeInfo *semantic_info_get_or_add(SemanticInfo *info, const tree_node_t *node) {
    
    for(size_t idx = 0; idx < info->count; idx++) {
        
        if(info->items[idx].node == node) {
            return &info->items[idx];
        }

        
    }

    //zvetseni pole kdyz uz neni misto
    if(info->count == info->capacity) {
        size_t new_capacity = 32;

        if(info->capacity != 0) {
            
            //kontrola preteceni
            size_t max_capacity = SIZE_MAX/sizeof(*info->items);
            if(info->capacity > max_capacity/2) {
                return NULL;
            }

            new_capacity = info->capacity * 2;
        }

        SemanticNodeInfo *new_items = realloc(info->items, new_capacity * sizeof(*new_items));
        if(new_items == NULL) {
            return NULL;
        }





        info->items = new_items;
        info->capacity = new_capacity;
    }



    SemanticNodeInfo *item = &info->items[info->count];
    item->node = node;
    item->type = TYPE_UNDEF;
    item->full_type.members = NULL;
    item->full_type.count = 0;
    item->declaration = NULL;
    item->scope_owner = NULL;
    item->promote_to_double = false ;

    info->count++;



    return item;
}


//prida typ do sjednoceni, pokud tam jeste neni
static int semantic_add_type_member(SemanticType *type, DataType member_type, const tree_node_t *declaration) {
    for(size_t idx = 0; idx < type->count; idx++) {
        
        if(type->members[idx].type==member_type && type->members[idx].declaration==declaration) {
            return 0;
        }
    }

    if(type->count >= SIZE_MAX/sizeof(*type->members)) {
        return 99;
    }

    SemanticTypeMember *members = realloc(type->members, (type->count + 1) * sizeof(*type->members));
    if(members == NULL) {
        return 99;
    }


    type->members = members;
    type->members[type->count].type = member_type;
    type->members[type->count].declaration = declaration;
    type->count++;

    return 0;
}




//nacte cleny typu(alias nahradi jeho cleny)
static int semantic_read_full_type(const tree_node_t *node, const SemanticInfo *info, SemanticType *type) {
    DataType member_type = TYPE_UNDEF;

    switch(node->rule) {
        case RULE_SIMPLE_TYPE_1:
            member_type = TYPE_INT;
            break;

        case RULE_SIMPLE_TYPE_2:
            member_type = TYPE_DOUBLE;
            break;

        case RULE_SIMPLE_TYPE_3:
            member_type = TYPE_STRING;
            break;

        case RULE_SIMPLE_TYPE_4:
            member_type = TYPE_UNIT;
            break;

        case RULE_SIMPLE_TYPE_5:
            member_type = TYPE_BOOL;
            break;

        case RULE_SIMPLE_TYPE_6: {
            const char *name = node->children[0]->token.data;

            for(size_t idx = 0; idx < info->count; idx++) {
                
                const SemanticNodeInfo *item = &info->items[idx];
                const tree_node_t *definition = item->node;

                
                if(definition->rule == RULE_CASE_CLASS_DEF) {
                    const char *class_name = definition->children[2]->token.data;

                    if(strcmp(class_name, name) == 0) {
                        
                        return semantic_add_type_member(type, TYPE_UNDEF, definition);
                    }

                    continue;
                }
                
                
                if(definition->rule != RULE_TYPE_DEF) {
                    continue;
                }

                if(strcmp(definition->children[1]->token.data, name) != 0) {
                    continue;
                }


                for(size_t position = 0; position < item->full_type.count; position++) {
                    const SemanticTypeMember *member = &item->full_type.members[position];

                    int error = semantic_add_type_member(type, member->type, member->declaration);
                    if(error != 0) {
                        return error;
                    }
                }


                return 0;
            }

            return 3;
        }

        case RULE_TYPE_2:
            return 6;


        default:
            break;
    }

    if(member_type != TYPE_UNDEF) {
        return semantic_add_type_member(type, member_type, NULL);
    }

    for(size_t idx = 0; idx < node->children_count; idx++) {
        const tree_node_t *child = node->children[idx];

        if(child->type == NODE_TERMINAL) {
            continue;
        }

        int error = semantic_read_full_type(child, info, type);
        if(error != 0) {
            return error;
        }
    }

    return 0;
}



//ulozi vlastni kopii celeho typu
static int semantic_store_full_type(SemanticInfo *info, const tree_node_t *node, const SemanticType *type) {
    SemanticType copy = {0};

    for(size_t idx = 0; idx < type->count; idx++) {
        int error = semantic_add_type_member(&copy, type->members[idx].type, type->members[idx].declaration);

        if(error != 0) {
            free(copy.members);
            return error;
        }
    }

    SemanticNodeInfo *item = semantic_info_get_or_add(info, node);
    if(item == NULL) {
        free(copy.members);
        return 99;
    }

    free(item->full_type.members);
    item->full_type = copy;
    item->type = TYPE_UNDEF;


    if(copy.count == 1) {
        item->type = copy.members[0].type;
    }

    return 0;
}


//najde cely typ vyrazu
static const SemanticType *semantic_find_full_type(const SemanticInfo *info, const tree_node_t *node) {
    if(node == NULL) {
        return NULL;
    }

    const SemanticNodeInfo *item = semantic_info_find(info, node);

    if(item!=NULL && item->full_type.count > 0) {
        return &item->full_type;
    }

    if(node->rule==RULE_EXPRESSION && node->children_count==1) {
        return semantic_find_full_type(info, node->children[0]);
    }

    return NULL;
}


//prevezme typ hodnoty, vcetne celeho sjednoceni
static int semantic_copy_value_type(SemanticInfo *info, const tree_node_t *node, const tree_node_t *value, DataType type) {
    const SemanticType *full_type = semantic_find_full_type(info, value);

    if(full_type != NULL) {
        return semantic_store_full_type(info, node, full_type);
    }

    if(type == TYPE_UNDEF) {
        return 0;
    }

    SemanticTypeMember member = {type, NULL};
    SemanticType simple ={&member, 1};

    return semantic_store_full_type(info, node, &simple);
}



//ulozi typ uvedeny v anotaci
static int semantic_store_annotation(SemanticInfo *info, const tree_node_t *node, const tree_node_t *annotation) {
    SemanticType type = {0};

    int error = semantic_read_full_type(annotation, info, &type);
    if(error == 0) {
        error = semantic_store_full_type(info, node, &type);
    }

    free(type.members);
    return error;
}



static int semantic_check_full_value(const SemanticType *expected, const tree_node_t *expression, DataType actual_type, SemanticInfo *info, int error_code) {
    SemanticTypeMember member = {actual_type, NULL};
    SemanticType simple = {&member, 1};

    const SemanticType *actual = semantic_find_full_type(info, expression);

    if(actual == NULL) {
        if(actual_type == TYPE_UNDEF) {
            return 0;
        }

        actual = &simple;
    }

    bool convert = false;


    for(size_t idx = 0; idx < actual->count; idx++) {
        bool found = false;

        for(size_t position = 0; position < expected->count; position++) {
            if(actual->members[idx].type==expected->members[position].type &&
               actual->members[idx].declaration==expected->members[position].declaration) {

                found = true;
                break;
            }
        }

        if(!found && actual->count == 1 && actual->members[idx].type == TYPE_INT) {
            for(size_t position = 0; position < expected->count; position++) {
                
                if(expected->members[position].type == TYPE_DOUBLE) {
                    found = true;
                    convert = true;
                    break;
                }
            }
        }

        if(!found) {
            return error_code;
        }
    }



    if(convert && expression!=NULL) {
        SemanticNodeInfo *item = semantic_info_get_or_add(info, expression);

        if(item == NULL) {
            return 99;
        }

        item->promote_to_double = true;
    }

    
    return 0;
}


//porovna hodnotu s typem uvedenym v anotaci
static int semantic_check_annotated_value(const tree_node_t *annotation,const tree_node_t *expression, DataType actual_type, SemanticInfo *info, int error_code, bool can_discard) {
    SemanticType expected = {0};

    int error = semantic_read_full_type(annotation, info, &expected);

    if(error == 0) {
        bool unit_result = expected.count==1 && expected.members[0].type==TYPE_UNIT;


        if(!can_discard || !unit_result) {
            error = semantic_check_full_value(&expected, expression, actual_type, info, error_code);
        }
    }

    free(expected.members);
    return error;
}


//ulozi aliasy v poradi v jakem jsou definovane
static int semantic_collect_type_aliases(const tree_node_t *node, SemanticInfo *info) {
    if(node == NULL) {
        return 0;
    }

    if(node->type==NODE_NONTERMINAL && node->rule==RULE_TYPE_DEF) {
        const char *name = node->children[1]->token.data;

        for(size_t idx = 0; idx < info->count; idx++) {
            const tree_node_t *definition = info->items[idx].node;


            if(definition->rule == RULE_CASE_CLASS_DEF) {
                if(strcmp(definition->children[2]->token.data, name)==0) {
                    return 4;
                }
            }

            if(definition->rule==RULE_TYPE_DEF && strcmp(definition->children[1]->token.data, name) == 0) {
                return 4;
            }
        }

        SemanticType type = {0};


        int error = semantic_read_full_type(node->children[3], info, &type);
        if(error != 0) {
            free(type.members);
            return error;
        }

        SemanticNodeInfo *item = semantic_info_get_or_add(info, node);
        if(item == NULL) {
            free(type.members);
            return 99;
        }


        free(item->full_type.members);
        item->full_type = type;
        return 0;
    }

    for(size_t idx = 0; idx < node->children_count; idx++) {
        int error = semantic_collect_type_aliases(node->children[idx], info);
        if(error != 0) {
            return error;
        }
    }

    return 0;
}


//ulozi nazvy trid, aby je slo pouzit v typech
static int semantic_collect_class_names(const tree_node_t *node, SemanticInfo *info) {
    if(node == NULL) {
        return 0;
    }

    if(node->type==NODE_NONTERMINAL && node->rule==RULE_CASE_CLASS_DEF) {
        const char *name = node->children[2]->token.data;

        for(size_t idx = 0; idx < info->count; idx++) {
            
            const tree_node_t *definition = info->items[idx].node;

            if(definition->rule != RULE_CASE_CLASS_DEF) {
                continue;
            }

            if(strcmp(definition->children[2]->token.data, name) == 0) {
                return 4;
            }
        }

        SemanticNodeInfo *item = semantic_info_get_or_add(info, node);
        if(item == NULL) {
            return 99;
        }

        return semantic_add_type_member(&item->full_type, TYPE_UNDEF, node);
    }

    for(size_t idx = 0; idx < node->children_count; idx++) {
        int error = semantic_collect_class_names(node->children[idx], info);

        if(error != 0) {
            return error;
        }
    }

    return 0;
}

//spocita parametry funkce v stromu
static size_t semantic_count_params(const tree_node_t *node) {
    
    if(node == NULL) {
        return 0;
    }


    if(node->type==NODE_NONTERMINAL && node->rule==RULE_PARAM) {
        return 1;
    }

    size_t count = 0;

    for(size_t idx = 0; idx < node->children_count; idx++) {
        count += semantic_count_params(node->children[idx]);

    }


    
    return count;
}


//¨vrati datovy typ ze stromu
static DataType semantic_get_type(const tree_node_t *node) {
    
    if(node == NULL) {
        return TYPE_UNDEF;
    }
    if(node->type != NODE_NONTERMINAL) {
        return TYPE_UNDEF;
    }
    if(node->rule != RULE_TYPE_1) {
        return TYPE_UNDEF;
    }
    if(node->children[1]->rule == RULE_UNION_REST_1) {
        return TYPE_UNDEF;
    }


    const tree_node_t *simple_type = node->children[0];

    switch(simple_type->rule) {
        case RULE_SIMPLE_TYPE_1:
            return TYPE_INT; 

        case RULE_SIMPLE_TYPE_2:
            return TYPE_DOUBLE;

        case RULE_SIMPLE_TYPE_3:
            return TYPE_STRING;

        case RULE_SIMPLE_TYPE_4:
            return TYPE_UNIT;

        case RULE_SIMPLE_TYPE_5:
            return TYPE_BOOL;


        default:
            return TYPE_UNDEF;
    }
}

//najde extension(ke ktere patri metoda)
static const tree_node_t *semantic_find_extension(const tree_node_t *node) {
    node = node->parent;

    while(node != NULL) {
        if(node->rule == RULE_EXTENSION_DEF) {
            return node;
        }

        node = node->parent;
    }

    return NULL;
}


//metody ruznych typu maji v tabulce ruzne klice
static char *semantic_function_key(const tree_node_t *function) {
    const char *name = function->children[1]->token.data;
    const tree_node_t *extension = semantic_find_extension(function);

    size_t size = strlen(name) + 32;
    char *key = malloc(size);

    if(key == NULL) {
        return NULL;
    }

    if(extension == NULL) {
        strcpy(key, name);
    }else {
        DataType type = semantic_get_type(extension->children[2]->children[2]);
        snprintf(key, size, "%d:%s", (int)type, name);
    }

    return key;
}


static Symbol *semantic_find_function_symbol(const SymTable *functions, const tree_node_t *function) {
    char *key = semantic_function_key(function);

    if(key == NULL) {
        return NULL;
    }

    Symbol *symbol = symtable_lookup(functions, key);
    free(key);

    return symbol;
}


static const tree_node_t *semantic_find_method(const SemanticInfo *info, const char *name, DataType receiver_type) {
    
    for(size_t idx = 0; idx < info->count; idx++) {
        const tree_node_t *function = info->items[idx].node;

        if(function->rule != RULE_FUNCTION_DEF) {
            continue;
        }

        const tree_node_t *extension = semantic_find_extension(function);
        if(extension == NULL) {
            continue;
        }

        DataType type = semantic_get_type(extension->children[2]->children[2]);

        if(type == receiver_type &&
           strcmp(function->children[1]->token.data, name) == 0) {
            return function;
        }
    }


    return NULL;
}

//ulozi typy parametru(pole)
static void semantic_get_param_types(const tree_node_t *node, DataType *param_types, size_t *position) {
    if(node == NULL) {
        return;
    }

    if(node->type==NODE_NONTERMINAL && node->rule==RULE_PARAM) {
        
        param_types[*position] = semantic_get_type(node->children[2]);
        (*position)++;
        
        
        return;
    }

    for(size_t idx = 0; idx < node->children_count; idx++) {
        
        semantic_get_param_types(node->children[idx], param_types, position);

    }
}



//vrati typ literalu podle tokenu
static DataType semantic_literal_type(const tree_node_t *node) {
    
    if(node == NULL) {
        return TYPE_UNDEF;
    }
    if(node->type != NODE_TERMINAL) {
        return TYPE_UNDEF;
    }

    switch(node->token.type) {
        case T_INT:
            return TYPE_INT;
        
        case T_DOUBLE:
            return TYPE_DOUBLE;

        case T_STRING:
            return TYPE_STRING;

        case T_UNIT_BRACKETS:
            return TYPE_UNIT;


        case KW_TRUE:
        case KW_FALSE:
            return TYPE_BOOL;

        default:
            return TYPE_UNDEF;
    }
}



//uklada typ k uzlu(pro generator)
static int semantic_save_type(SemanticInfo *info, const tree_node_t *node, DataType type) {
    return semantic_copy_value_type(info, node, NULL, type);
}



typedef struct SemanticScope {
    SymTable variables;
    const SymTable *functions;
    struct SemanticScope *parent;
    const struct SemanticScope *caller;
    const tree_node_t *owner;
} SemanticScope;



//vyhleda promennou v bloku(aktualni/nadrazeny )
static int semantic_resolve_variable(const tree_node_t *node, const SemanticScope *scope, SemanticInfo *info, DataType *result, bool *can_change) {
    const char *name = node->token.data;

    while(scope != NULL) {
        Symbol *symbol = symtable_lookup(&scope->variables, name);

        if(symbol != NULL) {
            
            //najde deklaraci promenne
            for(size_t idx = 0; idx < info->count; idx++) {
                
                const SemanticNodeInfo *item = &info->items[idx];
                const tree_node_t *declaration = item->node;
                const tree_node_t *declared_name = NULL;

                if(declaration->rule == RULE_VAR_DEFINITION) {
                    declared_name = declaration->children[1];

                }else if (declaration->rule==RULE_PARAM || declaration->rule==RULE_GENERATOR) {
                    declared_name = declaration->children[0];
                }

                if(declaration->rule == RULE_FUNCTION_DEF) {
                    
                    const tree_node_t *extension = semantic_find_extension(declaration);

                    if(extension != NULL) {
                        declaration = extension->children[2];
                        declared_name = declaration->children[0];
                    }
                }


                if(item->scope_owner == scope->owner && declared_name != NULL && strcmp(declared_name->token.data, name) == 0) {
                    
                    SemanticNodeInfo *use = semantic_info_get_or_add(info, node);
                    if (use == NULL) {
                        return 99;
                    }



                    *result = symbol->var.type;
                    use->type = *result;
                    use->declaration = declaration;

                    if(can_change != NULL) {
                        *can_change = symbol->var.is_mutable;
                    }

                    return semantic_copy_value_type(info, node, declaration, *result);
                }
            }


            return 99;
        }

        scope = scope->parent;
    }


    return 3;
}

typedef struct {
    size_t param_count;
    DataType param_types[3];
    DataType return_type;
    bool accepts_any_type;
} BuiltinFunction;

//vrati pocet param, typy vestavene funkce
static bool semantic_get_builtin(const char *name, BuiltinFunction *builtin) {
    builtin->accepts_any_type = false;

    if(strcmp(name, "print")==0) {
        builtin->param_count = 1;
        builtin->return_type = TYPE_UNIT;
        builtin->accepts_any_type = true;
        return true;

    }

    if(strcmp(name, "readLine")==0) {
        builtin->param_count = 0;
        builtin->return_type = TYPE_STRING;
        return true;
    }

    if(strcmp(name, "readInt")==0) {
        builtin->param_count = 0;
        builtin->return_type = TYPE_INT;
        return true;
    }

    if(strcmp(name, "readDouble")==0) {
        builtin->param_count = 0;
        builtin->return_type = TYPE_DOUBLE;
        return true;
    }


    if(strcmp(name, "readBoolean") == 0) {
        builtin->param_count = 0;
        builtin->return_type = TYPE_BOOL;
        return true;

    }


    if(strcmp(name, "toStr")==0) {
        builtin->param_count = 1;
        builtin->return_type = TYPE_STRING;
        builtin->accepts_any_type = true;
        return true;
    }

    if(strcmp(name, "toInt")==0) {
        builtin->param_count = 1;
        builtin->param_types[0] = TYPE_DOUBLE;
        builtin->return_type = TYPE_INT;
        return true;
    }

    if(strcmp(name, "strLength")==0) {
        builtin->param_count = 1;
        builtin->param_types[0] = TYPE_STRING;
        builtin->return_type = TYPE_INT;
        return true;
    }

    if(strcmp(name, "subStr")==0) {
        builtin->param_count = 3;
        builtin->param_types[0] = TYPE_STRING;
        builtin->param_types[1] = TYPE_INT;
        builtin->param_types[2] = TYPE_INT;
        builtin->return_type = TYPE_STRING;
        return true;
    }

    if(strcmp(name, "ord")==0) {
        builtin->param_count = 2;
        builtin->param_types[0] = TYPE_STRING;
        builtin->param_types[1] = TYPE_INT;
        builtin->return_type = TYPE_INT;
        return true;
    }


    if(strcmp(name, "chr")==0) {
        builtin->param_count = 1;
        builtin->param_types[0] = TYPE_INT;
        builtin->return_type = TYPE_STRING;
        return true;
    }

    return false;
}


static int semantic_check_variables(const tree_node_t *node, SemanticScope *scope, const SymTable *functions, SemanticInfo *info);


static int semantic_type_call(const tree_node_t *node, const SemanticScope *scope, SemanticInfo *info, DataType *result, const tree_node_t *method);


//podle clena a typu vlevo urci vysledny typ
static int semantic_get_member_type(const char *name, DataType receiver_type, DataType *result) {
    if(strcmp(name, "toString") == 0 ) {
        *result = TYPE_STRING;
        return 0;
    }

    if(strcmp(name, "length") == 0) {
        if(receiver_type != TYPE_STRING) {
            return 6;
        }

        *result = TYPE_INT;
        return 0;
    }

    if(strcmp(name, "toInt") == 0) {
        if(receiver_type != TYPE_DOUBLE) {
            return 6;
        }
        *result = TYPE_INT;
        return 0;
    }

    if(strcmp(name, "floor") == 0) {
        if(receiver_type != TYPE_DOUBLE) {
            return 6;
        }

        *result = TYPE_DOUBLE;
        return 0;
    }



    return 3;
}



static int semantic_type_if_value(const tree_node_t *node, const SemanticScope *scope, SemanticInfo *info, DataType *result);



static const tree_node_t *semantic_value_expression(const tree_node_t *value) {
    
    if(value->rule == RULE_VALUE_1) {
        return value->children[0];
    }

    if(value->rule==RULE_VALUE_2 && value->children[1]->rule==RULE_MATCH_SUFFIX_2) {
        
        return value->children[0] ;
    }


    return NULL;
}



//zjisti typ vyrazu, overi kompatibilitu operandu
static int semantic_type_expression(const tree_node_t *node, const SemanticScope *scope, SemanticInfo *info, DataType *result) {
    *result = TYPE_UNDEF;

    if(node == NULL) {
        return 0;
    }


    if(node->rule == RULE_VALUE_1) {
        node = node->children[0];
    }

    if(node->rule == RULE_IF_STATEMENT) {
        
        return semantic_type_if_value(node, scope, info, result);
    }


    if(node->type == NODE_TERMINAL) {
        
        if(node->token.type == T_ID){
            int error = semantic_resolve_variable(node, scope, info, result, NULL);
            if(error != 3) {
                return error;
            }

            return semantic_type_call(node, scope, info, result, NULL);
        }
                
        
        *result = semantic_literal_type(node);
        if(*result == TYPE_UNDEF) {
            return 0;
        }

        return semantic_save_type(info, node, *result);
    }


    if(node->rule == RULE_EXPRESSION_AFTER_ID) {
        return semantic_type_call(node, scope, info, result, NULL);
    }


    if(node->rule == RULE_EXPRESSION_UNARY) {
        const tree_node_t *operation = node->children[0];
        const tree_node_t *operand = node->children[1];

        if(operation->token.type != T_MINUS && operation->token.type != T_NEG) {
            return 0;
        }

        int error = semantic_type_expression(operand, scope, info, result);
        if(error != 0) {
            return error;
        }

        if(*result == TYPE_UNDEF) {
            if(semantic_find_full_type(info, operand) != NULL) {
                return 6;
            }

            return 0;
        }


        if(operation->token.type == T_NEG) {
            
            return semantic_save_type(info, node, *result);
        }


        return semantic_save_type(info, node, *result);
    }



    if(node->rule != RULE_EXPRESSION) {
        return 0;
    }


    if(node->children_count == 1) {
        int error = semantic_type_expression(node->children[0], scope, info, result);
        if(error!=0) {
            return error;
        }

        return semantic_copy_value_type(info, node, node->children[0], *result);
    }


    t_type operation = node->children[1]->token.type;
    
    if(operation == T_DOT) {
        const tree_node_t *receiver = node->children[0];
        const tree_node_t *member =node->children[2]->children[0];

        const char *name = member->token.data;

        if(member->type != NODE_TERMINAL) {
            name = member->children[0]->token.data;
        }

        DataType receiver_type;

        int error = semantic_type_expression(receiver, scope, info, &receiver_type);
        if(error != 0) {
            return error;
        }


        if(receiver_type == TYPE_UNDEF) {
            return 6;
        }

        int member_error = semantic_get_member_type(name, receiver_type, result);

        if(member->type==NODE_TERMINAL && member_error==0) {
            return semantic_save_type(info, node, *result);
        }

        const tree_node_t *method = semantic_find_method(info, name, receiver_type);

        if(method == NULL) {
            if(member->type!=NODE_TERMINAL && member_error!=3) {
                return 2;
            }

            return member_error;
        }

        error = semantic_type_call(member, scope, info, result, method);
        if(error != 0) {
            return error;
        }

        SemanticNodeInfo *item = semantic_info_get_or_add(info, node);
        if(item == NULL) {
            return 99;
        }


        item->type = *result;
        item->declaration = method;

        return 0;
    }
    
    
    
    bool is_comparison = false;
    bool is_equality = false;

    switch(operation) {
        case T_PLUS:
        case T_MINUS:
        case T_MULT:
        case T_DIV:
        case T_AND:
        case T_OR:
            break;

        case T_LT:
        case T_LOET:
        case T_GT:
        case T_GOET:
            is_comparison = true;
            break;

        case T_ET:
        case T_NET:
            is_comparison = true;
            is_equality = true;
            break;



        default:
            return 0;
    }

    const tree_node_t *left = node->children[0];
    const tree_node_t *right = node->children[2];
    DataType left_type;
    DataType right_type;

    int error = semantic_type_expression(left, scope, info, &left_type);
    if(error != 0) {
        return error;
    }
    error = semantic_type_expression(right, scope, info, &right_type);
    if(error != 0) {
        return error;
    }


    

    if(left_type==TYPE_UNDEF || right_type==TYPE_UNDEF) {
        if((left_type==TYPE_UNDEF && semantic_find_full_type(info, left)!=NULL) ||
           (right_type==TYPE_UNDEF && semantic_find_full_type(info, right)!=NULL)) {
            return 6;
        }


        return 0;
    }


    if(operation==T_AND || operation==T_OR) {
        
        if(left_type!=TYPE_BOOL || right_type!=TYPE_BOOL) {
            return 6;
        }


        *result = TYPE_BOOL;
        return semantic_save_type(info, node, *result);
    }

    if(operation==T_PLUS && left_type==TYPE_STRING && right_type==TYPE_STRING) {
        *result = TYPE_STRING;
        return semantic_save_type(info, node, *result);
    }

    if(is_equality && left_type == right_type) {
        *result = TYPE_BOOL;
        return semantic_save_type(info, node, *result);
    }


    if(left_type==TYPE_BOOL || right_type==TYPE_BOOL) {
        return 6;
    }

    if(is_comparison && left_type == TYPE_STRING && right_type == TYPE_STRING) {
        *result = TYPE_BOOL;
        return semantic_save_type(info, node, *result);
    }

    bool left_is_number = left_type==TYPE_INT || left_type==TYPE_DOUBLE;
    bool right_is_number = right_type==TYPE_INT || right_type==TYPE_DOUBLE;

    if(!left_is_number || !right_is_number) {
        return 6;
    }


    if(is_comparison) {
        *result = TYPE_BOOL;
    }else if (left_type==TYPE_DOUBLE || right_type==TYPE_DOUBLE) {
        *result = TYPE_DOUBLE;
    }else {
        *result = TYPE_INT;
    }


    //kdyz se typy lisi, generator musi prevest int na double
    if(left_type != right_type) {
        
        const tree_node_t *operand = right;
        
        if (left_type == TYPE_INT) {
            operand = left;
        }

        SemanticNodeInfo *item = semantic_info_get_or_add(info, operand);
        if (item == NULL) {
            return 99;
        }


        item->promote_to_double = true;
    }


    return semantic_save_type(info, node, *result);
}



//kontroluje volani funkce, argumenty, navratovy typ
static int semantic_type_call(const tree_node_t *node, const SemanticScope *scope, SemanticInfo *info, DataType *result, const tree_node_t *method) {
    
    if(scope == NULL) {
        return 0;
    }

    const char *name;
    size_t list_count = 0;

    if(node->type == NODE_TERMINAL) {
        
        name = node->token.data;

    }else{
        name = node->children[0]->token.data;
        list_count = node->children_count - 1;
    }


    const tree_node_t *definition = method;
    const DataType *param_types;
    size_t param_count;
    DataType return_type;
    bool accepts_any_type = false;

    BuiltinFunction builtin;
    Symbol *function;

    if(method == NULL) {
        function = symtable_lookup(scope->functions, name);

    }else {
        function = semantic_find_function_symbol(scope->functions, method);

        if(function == NULL) {
            return 99;
        }
    }

    if(function == NULL || function->type != SYM_FUNC) {
        if(!semantic_get_builtin(name, &builtin)) {
            return 3;
        }

        param_types = builtin.param_types;
        param_count = builtin.param_count;
        return_type = builtin.return_type;
        accepts_any_type = builtin.accepts_any_type;
    }else {
        for(size_t idx = 0; idx < info->count; idx++) {
            const tree_node_t *candidate = info->items[idx].node;

            if(definition==NULL && candidate->rule==RULE_FUNCTION_DEF && semantic_find_extension(candidate)==NULL && strcmp(candidate->children[1]->token.data, name)==0) {
                definition = candidate;
                break;
            }
        }

        if(definition == NULL) {
            return 99;
        }

        

        param_types = function->func.param_types;
        param_count = function->func.param_count;
        return_type = function->func.return_type;
    }


    //porovna rozdeleni argumentu do seznamu
    if(definition == NULL) {
        
        if(list_count!=1) {
            return 5;
        }

    }else {
        const tree_node_t *params = definition->children[2];
        size_t list_index = 0;

        while(params->rule == RULE_PARAM_LISTS_1) {
            if(list_index >= list_count) {
                return 5;
            }

            const tree_node_t *next_list = params->children[params->children_count - 1];

            size_t expected_count = semantic_count_params(params);
            expected_count -= semantic_count_params(next_list);

            const tree_node_t *args = node->children[list_index + 1];
            size_t actual_count = 0;


            for(size_t idx = 0; idx < args->children_count; idx++) {
                
                if(args->children[idx]->type == NODE_NONTERMINAL) {
                    actual_count++;
                }
            }

            if(actual_count != expected_count) {
                return 5;
            }

            params = next_list;
            list_index++;
        }
    }



    size_t arg_index = 0;
    
    for(size_t list_index = 0; list_index < list_count; list_index++) {
        const tree_node_t *args = node->children[list_index + 1];

        for(size_t idx = 0; idx < args->children_count; idx++) {
    
            const tree_node_t *value = args->children[idx];

            if(value->type == NODE_TERMINAL) {
                continue;
            }

            const tree_node_t *expression = semantic_value_expression(value);
            if(expression == NULL) {
                return 0;
            }


            if(arg_index >= param_count) {
                return 5;
            }

            DataType actual;

            int error = semantic_type_expression(expression, scope, info, &actual);
            if(error != 0) {
                return error;
            }


        

            if(actual == TYPE_UNDEF) {
                return 0;
            }


            if(accepts_any_type) {
                arg_index++;
                continue;
            }

            DataType expected = param_types[arg_index];
            if(expected == TYPE_UNDEF) {
                return 0;
            }

            if(expected==TYPE_DOUBLE && actual==TYPE_INT) {
                
                SemanticNodeInfo *argument =semantic_info_get_or_add(info, expression);
                if (argument == NULL) {
                    return 99;
                }

                argument->promote_to_double = true;

            }else if (expected != actual) {
                return 5;
            }

            arg_index++;
        }
    }


    if(arg_index != param_count) {
        return 5;
    }

    if(definition!=NULL && return_type==TYPE_UNDEF && definition->children[3]->rule==RULE_OPTIONAL_TYPE_2) {

        const SemanticScope *current = scope;
        while(current != NULL) {
            
            if(current->owner == definition) {
                return 6;
            }

            if(current->owner->rule == RULE_FUNCTION_DEF) {
                current = current->caller;
            }else {
                current = current->parent;
            }

        }

        int error = semantic_check_variables(definition, (SemanticScope *)scope, scope->functions, info);
        if(error != 0) {
            return error;
        }


        return_type = function->func.return_type;
    }


    SemanticNodeInfo *item = semantic_info_get_or_add(info, node);
    if(item == NULL) {
        return 99;
    }

    *result = return_type;
    item->type = *result;
    item->declaration = definition;



    return 0;
}


//kontroluje navratovy typ hlavni funkce
static int semantic_check_main_type(const tree_node_t *node) {
    
    
    if (node->rule == RULE_OPTIONAL_TYPE_2) {
        return 0;
    }

    const tree_node_t *type_node = node->children[1];


    if(type_node->rule == RULE_TYPE_2) {
        return 6;
    }

    DataType type = semantic_get_type(type_node);

    

    if(type == TYPE_UNDEF) {
        return 0;
    }
    if(type != TYPE_UNIT) {
        return 6;
    }

    return 0;
}



//nacte infor o parametrech, navratovem typu funkce
static int semantic_get_function_data(const tree_node_t *node, FuncData *data) {
    
    data->param_types = NULL;
    data->param_count = 0;
    data->return_type = TYPE_UNDEF;

    const tree_node_t *params =node->children[2];
    const tree_node_t *return_type = node->children[3];

    data->param_count = semantic_count_params(params);


    if(data->param_count > 0) {
        

        data->param_types = malloc(data->param_count * sizeof(*data->param_types ));
        if (data->param_types == NULL) {
            data->param_count = 0;
            return 99;
        }


        size_t position = 0;
        semantic_get_param_types(params, data->param_types, &position);
    }



    if(return_type->rule == RULE_OPTIONAL_TYPE_1) {
        data->return_type = semantic_get_type(return_type->children[1]);
    }

    return 0;
}



//funkce do tabulky symbolu
static int semantic_register_function(const tree_node_t *node, SymTable *functions, SemanticInfo *info) {
    const tree_node_t *extension = semantic_find_extension(node);

    if(extension != NULL &&
       
        semantic_get_type(extension->children[2]->children[2])==TYPE_UNDEF) {
        return 6;
    }

    FuncData data;

    int error = semantic_get_function_data(node, &data);
    if(error != 0) {
        return error;
    }

    char *key =semantic_function_key(node);
    if(key == NULL) {
        free(data.param_types);
        return 99;
    }


    int result = symtable_insert(functions, key);
    Symbol *symbol = NULL;

    if(result == RET_OK) {
        symbol = symtable_lookup(functions, key);
    }

    free(key);

    if(result != RET_OK) {
        free(data.param_types);

        if(result == RET_DUPLICATE) {
            return 4;
        }

        return 99;
    }

    if(symbol == NULL) {
        free(data.param_types);
        return 99;
    }

    symbol->type = SYM_FUNC;
    symbol->func = data;

    SemanticNodeInfo *item = semantic_info_get_or_add(info, node);
    if(item == NULL) {
        return 99;
    }

    item->type = data.return_type;

    if(extension != NULL) {
        item->scope_owner = node;
    }

    return 0;
}


//projde strom, ulozi definice funkci, spocita hlavni funkce
static int semantic_collect_functions(const tree_node_t *node, SymTable *functions, size_t *main_count, SemanticInfo *info) {
    
    if (node == NULL) {
        return 0;
    }

    int error = 0;


    if(node->type == NODE_NONTERMINAL) {
        
        switch(node->rule) {
            case RULE_TOP_ITEM_1:
                (*main_count)++;

                if(*main_count > 1) {
                    return 4;
                }

                return semantic_register_function(
                    node->children[2], functions, info
                );

            case RULE_TOP_ITEM_2:
                return semantic_register_function(
                    node->children[0], functions, info
                );


            case RULE_FUNCTION_DEF:
                if(semantic_find_extension(node) != NULL) {
                    return semantic_register_function(node, functions, info);
                }

                break;

            default:
                break;
        }
    }

    for (size_t idx = 0; idx < node->children_count; idx++) {
       
        error = semantic_collect_functions(node->children[idx], functions, main_count, info);
        if(error != 0) {
            return error;
        }
    }

    return 0;
}


//kontroluje typ hodnoty vracene return
static int semantic_check_return(const tree_node_t *node,const tree_node_t *return_type) {
    //return vyzaduje uvedeny navratovy typ
    if(return_type->rule == RULE_OPTIONAL_TYPE_2) {
        return 6;
    }

    const tree_node_t *value = node->children[1];


    //typ vracene hodnoty se kontroluje pozdeji v semantic_check_return_value
    if(value->rule == RULE_RETURN_VALUE_1) {
        return 0;
    }

    const tree_node_t *type_node = return_type->children[1];

    if(type_node->rule == RULE_TYPE_2) {
        return 6;
    }

    DataType type = semantic_get_type(type_node);

    if(type == TYPE_UNDEF) {
        return 0;
    }


    return 0;
}


static int semantic_check_returns(const tree_node_t *node,const tree_node_t *return_type) {
    
    if(node == NULL) {
        return 0;
    }

    int error = 0;

    if(node->type == NODE_NONTERMINAL) {
        //do vnorene funkce nejde, ma vlastni navratovy typ
        if(node->rule == RULE_FUNCTION_DEF) {
            return 0;
        }

        if(node->rule == RULE_RETURN_STATEMENT) {
            
            error = semantic_check_return(node, return_type);
            if (error != 0) {
                return error;
            }
        }
    }

    for (size_t idx = 0; idx < node->children_count; idx++) {
        error = semantic_check_returns(node->children[idx], return_type);
        if(error != 0) {
            return error;
        }
    }

    return 0;
}


//kontroluje hlavni funkci, navratove hodnoty
static int semantic_check_node(const tree_node_t *node) {
    if(node == NULL) {
        return 0;
    }

    int error = 0;

    if(node->type == NODE_NONTERMINAL) {
        switch(node->rule) {
            case RULE_TOP_ITEM_1:
                
                if(semantic_count_params(node->children[2]->children[2]) != 0) {
                    return 5;
                }

                error = semantic_check_main_type(node->children[2]->children[3]);
                if(error != 0) {
                    return error;
                }

                break;

            case RULE_FUNCTION_DEF:
                error = semantic_check_returns(node->children[5], node->children[3]);
                if(error != 0) {
                    return error;
                }



                break;




            default:
                break;
        }
    }

    for (size_t idx = 0; idx < node->children_count; idx++) {
        error = semantic_check_node(node->children[idx]);

        if (error != 0) {
            return error;
        }
    }

    return 0;
}



//kontroluje inicializaci promene, da ji do tabulky
static int semantic_check_variable(const tree_node_t *node, SemanticScope *scope, SemanticInfo *info) {
    
    const tree_node_t *name = node->children[1];
    const tree_node_t *annotation = node->children[2];
    const tree_node_t *rhs = node->children[4];

    if(rhs->rule != RULE_RHS_2) {
        return 0;
    }


    const tree_node_t *value = rhs->children[0];
    
    const tree_node_t *expression = semantic_value_expression(value);
    if(expression == NULL) {
        return 0;
    }
    DataType type;
    int error = semantic_type_expression(expression, scope, info, &type);
    if(error != 0) {
        return error;
    }

    if(type==TYPE_UNDEF && semantic_find_full_type(info, expression)==NULL) {
        return 0;
    }
    


        if(annotation->rule == RULE_OPTIONAL_TYPE_1) {
            error = semantic_check_annotated_value(annotation->children[1], expression, type, info, 6, false);
            if(error != 0) {
                return error;
            }
        }

    //promennou pridava az po kontrole hodnoty(aby se nevidela ve vlastni deklaraci0
    int inserted = symtable_insert(&scope->variables, name->token.data);
    if(inserted == RET_DUPLICATE) {
        return 4;
    }
    if(inserted != RET_OK) {
        return 99;
    }

    Symbol *symbol = symtable_lookup(&scope->variables, name->token.data);
    if(symbol == NULL) {
        return 99;
    }

    symbol->type = SYM_VAR;
    symbol->var.type = type;
    symbol->var.is_mutable = false;

    if(node->children[0]->rule == RULE_VAR_KIND_2) {
        symbol->var.is_mutable = true;
    }

    SemanticNodeInfo *item = semantic_info_get_or_add(info, node);
    if(item == NULL) {
        return 99;
    }

    item->type = type;
    item->scope_owner = scope->owner;

    if(annotation->rule == RULE_OPTIONAL_TYPE_1) {
        error = semantic_store_annotation(info, node, annotation->children[1]);

    }else {
        error = semantic_copy_value_type(info, node, expression, type);
    }

    if(error != 0) {
        return error;
    }



    symbol->var.type = semantic_info_find(info, node)->type;
    return 0;
}


//overi ze jde do promenne priradit hodnotu daneho typu
static int semantic_check_assignment(const tree_node_t *node, const SemanticScope *scope, SemanticInfo *info) {
    
    const tree_node_t *name = node->parent->children[0];
    const tree_node_t *rhs = node->children[1];

    DataType variable_type;
    bool can_change;

    int error = semantic_resolve_variable(name, scope, info, &variable_type, &can_change);
    //leva strana musi byt uz definovana a musi jit menit
    if(error == 3) {
        return 7;
    }
    if(error != 0) {
        return error;
    }
    if(!can_change) {
        return 7;
    }

    if(rhs->rule != RULE_RHS_2) {
        return 0;
    }


    const tree_node_t *value = rhs->children[0];
    const tree_node_t *expression = semantic_value_expression(value);
    if(expression == NULL) {
        return 0;
    }
    DataType value_type;
    
    error = semantic_type_expression(expression, scope, info, &value_type);
    if(error != 0) {
        return error;
    }
    
    
    const SemanticType *expected = semantic_find_full_type(info, name);

    if(expected == NULL) {
        if(variable_type == TYPE_UNDEF) {
            return 0;
        }

        SemanticTypeMember member = {variable_type, NULL};
        SemanticType simple = {&member, 1};

        return semantic_check_full_value(&simple, expression, value_type, info, 6);
    }

    
    
    return semantic_check_full_value( expected, expression, value_type, info, 6);
    
}


//prida parametr funkce mezi promenne
static int semantic_bind_parameters(const tree_node_t *node, SemanticScope *scope, SemanticInfo *info) {
    if(node->type != NODE_NONTERMINAL) {
        return 0;
    }

    if(node->rule==RULE_PARAM || node->rule==RULE_GENERATOR) {
        const char *name = node->children[0]->token.data;
        DataType type = TYPE_INT;

        if(node->rule == RULE_PARAM) {
            type = semantic_get_type(node->children[2]);

        }

        int inserted = symtable_insert(&scope->variables, name);
        if(inserted == RET_DUPLICATE) {
            return 4;
        }
        if(inserted != RET_OK) {
            return 99;
        }

        Symbol *symbol = symtable_lookup(&scope->variables, name);
        if(symbol == NULL) {
            return 99;
        }

        symbol->type = SYM_VAR;
        symbol->var.type = type;
        symbol->var.is_mutable = false;

        SemanticNodeInfo *item = semantic_info_get_or_add(info, node);
        if(item == NULL) {
            return 99;
        }

        item->type = type;
        item->scope_owner = scope->owner;
        return 0;
    }

    for(size_t idx = 0; idx < node->children_count; idx++) {

        int error = semantic_bind_parameters(node->children[idx], scope, info);
        if(error != 0) {
            return error;
        }
    }

    return 0;
}


//podminka v if/while musi byt bool
static int semantic_check_condition(const tree_node_t *node, const SemanticScope *scope, SemanticInfo *info) {
    
    const tree_node_t *condition = node->children[1];
    DataType condition_type;

    int error = semantic_type_expression(condition,scope, info, &condition_type);
    if(error != 0) {
        return error;
    }

    if(condition_type != TYPE_BOOL ) {
        return 6;
    }



    return 0;
}


//kontroluje vyraz pouzity jako prikaz
static int semantic_check_expression_statement(const tree_node_t *node, const SemanticScope *scope, SemanticInfo *info) {
    
    if(node->children[1]->rule != RULE_MATCH_SUFFIX_2) {
        return 0;
    }

    const tree_node_t *expression = node->children[0];
    DataType type;
    
    int error = semantic_type_expression(expression, scope,info, &type);
    if(error != 0) {
        return error;
    }




    return 0;
}



//zkontroluje typ hodnoty vracene return
static int semantic_check_return_value(const tree_node_t *node, const SemanticScope *scope, SemanticInfo *info) {
    
    const tree_node_t *return_value = node->children[1];

    if(return_value->rule != RULE_RETURN_VALUE_1) {
        return 0;
    }

    const SemanticScope *return_scope = scope;

    
    //z bloku se vrati k funkci, ve ktere je return
    while(scope != NULL && scope->owner->rule != RULE_FUNCTION_DEF) {
        scope = scope->parent;
    }

    if(scope == NULL) {
        return 10;
    }


    const tree_node_t *return_type = scope->owner->children[3];
    if(return_type->rule != RULE_OPTIONAL_TYPE_1) {
        return 6;
    }

    const tree_node_t *value = return_value->children[0];
    
    const tree_node_t *expression = semantic_value_expression(value);
    if(expression == NULL) {
        return 0;
    }
    DataType expected_type = semantic_get_type(return_type->children[1]);
    DataType actual_type;

    int error = semantic_type_expression(expression, return_scope, info, &actual_type);
    if(error != 0) {
        return error;
    }


   
    if(actual_type==TYPE_UNDEF || expected_type==TYPE_UNDEF) {
        return 0;
    }


    if(expected_type==TYPE_UNIT || expected_type==actual_type) {
        return semantic_save_type(info, node, expected_type);
    }

    if(expected_type==TYPE_DOUBLE && actual_type==TYPE_INT) {
        
        SemanticNodeInfo *item = semantic_info_get_or_add(info, expression);
        if(item == NULL) {
            return 99;
        }


        item->promote_to_double = true;
        return semantic_save_type(info, node, expected_type);
    }



    return 6;
}


//kontroluje typ vysledku jednoradkove funkce
static int semantic_check_inline_body(const tree_node_t *function, const SemanticScope *scope, SemanticInfo *info) {
    
    const tree_node_t *rhs = function->children[5];
    if(rhs->rule != RULE_RHS_2) {
        return 0;
    }


    const tree_node_t *value = rhs->children[0];
    const tree_node_t *expression = semantic_value_expression(value);
    if(expression == NULL) {
        return 0;
    }
    DataType actual_type;

    int error = semantic_type_expression(expression, scope, info, &actual_type);
    if(error != 0) {
        return error;
    }



    const tree_node_t *return_type = function->children[3];
    if(return_type->rule != RULE_OPTIONAL_TYPE_1) {
        return 0;
    }

    DataType expected_type = semantic_get_type(return_type->children[1]);
    
    if(actual_type==TYPE_UNDEF || expected_type==TYPE_UNDEF) {
        return 0;
    }

    if(expected_type==TYPE_UNIT || expected_type==actual_type) {
        return 0;
    }

    if(expected_type==TYPE_DOUBLE && actual_type==TYPE_INT) {
        
        SemanticNodeInfo *item = semantic_info_get_or_add(info, expression);
        if(item == NULL) {
            return 99;
        }


        item->promote_to_double = true;
        return 0;
    }


    return 6;
}



static const tree_node_t *semantic_last_statement(const tree_node_t *block) { 
    
    const tree_node_t *list = block->children[2];

    while(list->children[1]->rule == RULE_STATEMENT_LIST_REST_1) {
        list = list->children[1];
    }

    const tree_node_t *line = list->children[0];
    const tree_node_t *statement = line->children[0];
    const tree_node_t *rest = line->children[1];


    while(rest->rule == RULE_STATEMENT_LINE_REST_1) {
        statement = rest->children[1];
        rest = rest->children[2];
    }


    return statement;
}

//urci vysledny typ funkce
static int semantic_get_body_result(const tree_node_t *node, SemanticInfo *info, DataType expected_type, const tree_node_t **expression, DataType *type, bool *ends_with_return) {

    *expression = NULL;
    *type = TYPE_UNIT;
    *ends_with_return = false;


    if(node->rule == RULE_BLOCK) {
        node = semantic_last_statement(node);
    }
    if(node->rule == RULE_STATEMENT_1) {
        return 0;
    }
    if(node->rule == RULE_STATEMENT_2) {
        node = node->children[0];
    }
    if(node->rule == RULE_INLINE_STATEMENT_1) {
        *ends_with_return = true;
        return 0;
    }
    if(node->rule == RULE_INLINE_STATEMENT_2) {
        if(expected_type == TYPE_UNIT) {
            return 0;
        }

        const tree_node_t *if_node = node->children[0];
        const tree_node_t *then_body = if_node->children[3];
        const tree_node_t *else_part = if_node->children[4];

        const tree_node_t *branch_expression;
        DataType then_type;
        bool then_returns;

        int error = semantic_get_body_result(then_body->children[0], info, expected_type, &branch_expression, &then_type, &then_returns);
        if(error != 0) {
            return error;
        }

        DataType else_type = TYPE_UNIT;
        bool else_returns = false;

        if(else_part->rule == RULE_ELSE_PART_1) {
            const tree_node_t *else_body = else_part->children[1];

           
            error = semantic_get_body_result(else_body->children[0], info, expected_type, &branch_expression, &else_type, &else_returns);
            if(error != 0) {
                return error;
            }
        }

        if(then_returns && else_returns) {
            *ends_with_return = true;
            return 0;
        }
        if(expected_type==TYPE_UNDEF && then_type==TYPE_UNIT && else_type==TYPE_UNIT) {
            return 0;
        }

        return 6;
    }


    if(node->rule == RULE_INLINE_STATEMENT_5) {
        node = node->children[node->children_count - 1];
    }
    if(node->rule==RULE_ID_STATEMENT_2 || node->rule==RULE_INLINE_STATEMENT_6 || node->rule==RULE_BODY_3){
        
        *expression = node->children[0];

        const SemanticNodeInfo *item = semantic_info_find(info, *expression);
        if(item==NULL || item->type==TYPE_UNDEF) {
            return 6;
        }

        *type = item->type;
    }

    return 0;
}



static int semantic_merge_if_types(const tree_node_t *node, SemanticInfo *info, DataType *result, bool *ends_with_return);


static int semantic_get_branch_type(const tree_node_t *body, SemanticInfo *info, const tree_node_t **expression, DataType *type, bool *ends_with_return) {

    const tree_node_t *statement = body->children[0];

    if(body->rule == RULE_BODY_3) {
        statement = body;
    }else if (statement->rule == RULE_BLOCK) {

        statement = semantic_last_statement(statement);
    }

    if(statement->rule == RULE_STATEMENT_2) {
        statement = statement->children[0];
    }


    if(statement->rule == RULE_INLINE_STATEMENT_2) {
        const tree_node_t *nested_if = statement->children[0];

        if(body->rule!=RULE_BODY_1 || nested_if->children[4]->rule==RULE_ELSE_PART_1) {
            *expression = nested_if;
            return semantic_merge_if_types(nested_if, info, type, ends_with_return);
        }
    }

    return semantic_get_body_result(statement, info, TYPE_UNIT, expression, type, ends_with_return);
}

static int semantic_merge_if_types(const tree_node_t *node, SemanticInfo *info,
    DataType *result, bool *ends_with_return) {

    const tree_node_t *else_part = node->children[4];
    if(else_part->rule != RULE_ELSE_PART_1) {
        return 2;
    }

    const tree_node_t *then_expression;
    const tree_node_t *else_expression;
    DataType then_type;
    DataType else_type;
    bool then_returns;
    bool else_returns;

    int error = semantic_get_branch_type(node->children[3], info, &then_expression, &then_type, &then_returns);
    if(error != 0) {
        return error;
    }

    error = semantic_get_branch_type(else_part->children[1], info, &else_expression, &else_type, &else_returns);
    if(error != 0) {
        return error;
    }

    *ends_with_return = then_returns && else_returns;

    if(then_returns) {
        *result = else_type;
    }else if (else_returns || then_type==else_type) {
        
        *result = then_type;
    }else {
        
        const tree_node_t *expression_to_convert;

        if(then_type==TYPE_INT && else_type==TYPE_DOUBLE) {
            expression_to_convert = then_expression;
        
        }else if(then_type==TYPE_DOUBLE && else_type==TYPE_INT) {
            expression_to_convert = else_expression;
        
        }else {
            return 6;
        }

        SemanticNodeInfo *item = semantic_info_get_or_add(info, expression_to_convert);
        if(item == NULL) {
            return 99;
        }

        item->promote_to_double = true;
        *result = TYPE_DOUBLE;
    }


    return semantic_save_type(info, node, *result);
}


static int semantic_type_if_value(const tree_node_t *node,
    const SemanticScope *scope, SemanticInfo *info, DataType *result) {


    if(node->children[4]->rule != RULE_ELSE_PART_1) {
        return 2;
    }

    int error = semantic_check_variables(node, (SemanticScope *)scope, scope->functions, info);
    if(error != 0) {
        return error;
    }

    bool ends_with_return;


    return semantic_merge_if_types(node, info, result, &ends_with_return);
}


//kontroluje/odvodi navratovy typ
static int semantic_finish_function(const tree_node_t *function, const SymTable *functions, SemanticInfo *info) {

    const tree_node_t *annotation = function->children[3];
    const tree_node_t *rhs = function->children[5];
    const tree_node_t *expression = NULL;
    DataType expected_type = TYPE_UNDEF;
    DataType actual_type;
    bool ends_with_return = false;

    if(annotation->rule == RULE_OPTIONAL_TYPE_1) {
        expected_type = semantic_get_type(annotation->children[1]);
    }

    if(rhs->rule == RULE_RHS_2) {
        
        if(annotation->rule == RULE_OPTIONAL_TYPE_1) {
            return 0;
        }

        const tree_node_t *value = rhs->children[0];
        

        expression = semantic_value_expression(value);
        if(expression == NULL) {
            return 6;
        }


        expression = value->children[0];
        const SemanticNodeInfo *item = semantic_info_find(info, expression);
        if(item == NULL || item->type == TYPE_UNDEF) {
            return 6;
        }

        actual_type = item->type;

    }else {
        
        int error = semantic_get_body_result(rhs->children[0], info, expected_type, &expression, &actual_type, &ends_with_return);
        if(error != 0) {
            return error;
        }
    }

    if(ends_with_return) {
        return 0;
    }
 

    if(annotation->rule == RULE_OPTIONAL_TYPE_2) {

        int error = semantic_save_type(info, function, actual_type);
        if(error != 0) {
            return error;
        }

        Symbol *symbol = semantic_find_function_symbol(functions, function);
        if(symbol == NULL) {
            return 99;
        }



        symbol->func.return_type = actual_type;
        return 0;
    }


    if(expected_type==TYPE_UNIT || expected_type==actual_type) {
        return 0;
    }

    if(expected_type==TYPE_DOUBLE && actual_type==TYPE_INT && expression!=NULL) {
        SemanticNodeInfo *item = semantic_info_get_or_add(info, expression);
        if(item == NULL) {
            return 99;
        }

        item->promote_to_double = true;
        return 0;
    }

    return 6;
}


static int semantic_check_range_int(const tree_node_t *expression, const SemanticScope *scope, SemanticInfo *info) {

    DataType type;
    int error = semantic_type_expression(expression, scope, info, &type);
    if(error != 0) {
        return error;
    }

    if(type != TYPE_INT) {
        return 6;
    }

    return 0;
}


static bool semantic_is_zero_literal(const tree_node_t *node) {
    
    if(node->rule==RULE_EXPRESSION && node->children_count==1) {
        return semantic_is_zero_literal(node->children[0]);
    }


    if(node->type!=NODE_TERMINAL || node->token.type!=T_INT) {
        return false;
    }

    const char *number = node->token.data;

    if(number[0] == '\0') {
        return false;
    }

    for(size_t idx = 0; number[idx] != '\0'; idx++) {
        
        if(number[idx] != '0' && number[idx] != '_') {
            return false;
        }
    }



    return true;
}



static int semantic_check_for(const tree_node_t *node, SemanticScope *scope,
    const SymTable *functions, SemanticInfo *info) {

    const tree_node_t *header = node->children[1];
    const tree_node_t *generator = header->children[0];

    if(header->rule == RULE_FOR_HEADER_2) {
        generator = header->children[2];
    }

    
    int error = semantic_check_range_int(generator->children[2], scope, info);
    if(error != 0) {
        return error;
    }

    error = semantic_check_range_int(generator->children[4], scope,info);
    if(error != 0) {
        return error;
    }

    const tree_node_t *step = generator->children[5];
    if(step->rule == RULE_STEP_1) {
        if(generator->children[3]->rule != RULE_RANGE_OP_1) {
            return 2;
        }

        const tree_node_t *step_expression = step->children[1];
        
        error = semantic_check_range_int(step_expression, scope, info);
        if(error != 0) {
            return error;
        }

        if(semantic_is_zero_literal(step_expression)) {
            return 10;
        }
    }

    SemanticScope local;
    if(!symtable_init(&local.variables)) {
        return 99;
    }

    local.functions = functions;
    local.parent = scope;
    local.caller = NULL;
    local.owner = node;

    
    error = semantic_bind_parameters(generator, &local, info);
    if(error == 0) {
        error = semantic_check_variables(header, &local, functions, info);
    }
    if(error == 0) {
        error = semantic_check_variables(node->children[3], &local, functions, info);
    }

    symtable_free(&local.variables);
    return error;
}



//kazda funkce/blok ma vlastni tabulku promennych
static int semantic_check_variables(const tree_node_t *node, SemanticScope *scope, const SymTable *functions,SemanticInfo *info) {
    if(node == NULL) {
        return 0;
    }
    if(node->type != NODE_NONTERMINAL) {
        return 0;
    }

    if(node->rule == RULE_VALUE_1) {
        return 0;
    }

    if(node->rule == RULE_FOR_STATEMENT) {
        return semantic_check_for(node, scope, functions, info);
    }


    if(node->rule == RULE_BODY_3) {
        DataType type;
        return semantic_type_expression(node->children[0], scope, info, &type);

    }


    if(node->rule==RULE_FUNCTION_DEF && node->children[3]->rule==RULE_OPTIONAL_TYPE_2) {
        const SemanticNodeInfo *item = semantic_info_find(info, node);

        if(item!=NULL && item->type!=TYPE_UNDEF) {
            return 0;
        }
    }

    SemanticScope local;
    bool opens_scope = false;
    if(node->rule==RULE_BLOCK || node->rule==RULE_FUNCTION_DEF) {
        opens_scope = true;
    }

    if(opens_scope) {
        if(!symtable_init(&local.variables)) {
            return 99;
        }


        local.functions = functions;
        local.parent = scope;
        local.caller = NULL;

        if(node->rule == RULE_FUNCTION_DEF) {
            local.caller = scope;
            local.parent = NULL;
        }


        local.owner = node;
        scope = &local;
    }


    int error = 0;

    if(node->rule == RULE_FUNCTION_DEF) {
        const tree_node_t *extension = semantic_find_extension(node);

        if(extension != NULL) {
            const tree_node_t *receiver = extension->children[2];
            const char *name = receiver->children[0]->token.data;

            int inserted = symtable_insert(&scope->variables, name);

            if(inserted != RET_OK) {
                error = 99;
            }else {
                Symbol *symbol = symtable_lookup(&scope->variables, name);

                if(symbol == NULL) {
                    error = 99;

                }else {
                    symbol->type = SYM_VAR;
                    symbol->var.type = semantic_get_type(receiver->children[2]);
                    symbol->var.is_mutable = false;
                }
            }
        }

        if(error == 0) {
            error = semantic_bind_parameters(node->children[2], scope, info);
        }

        if(error == 0) {
            error = semantic_check_inline_body(node, scope, info);
        }

    }else if (node->rule==RULE_VAR_DEFINITION && scope!=NULL) {
        error = semantic_check_variable(node, scope, info);

    }else if (node->rule==RULE_ID_STATEMENT_1 && scope!=NULL) {
        error = semantic_check_assignment(node, scope, info);

    }else if (node->rule==RULE_IF_STATEMENT || node->rule==RULE_WHILE_STATEMENT || node->rule==RULE_INLINE_GUARDS_1 || node->rule==RULE_GUARD_LINES_1) {
        error = semantic_check_condition(node, scope, info);
    }else if (node->rule == RULE_ID_STATEMENT_2 || node->rule == RULE_INLINE_STATEMENT_6) {
        error = semantic_check_expression_statement(node, scope, info);
    }else if (node->rule == RULE_RETURN_STATEMENT) {
        error = semantic_check_return_value(node, scope,info);
    }

    //projde potomky,ale pri prvni chybe skonci
    for(size_t idx = 0; idx < node->children_count; idx++) {
        if(error != 0) {
            break;
        }
        error = semantic_check_variables(node->children[idx], scope, functions, info);
    }

    if(error==0 && node->rule==RULE_FUNCTION_DEF) {
        error = semantic_finish_function(node, functions, info);
    }

    if(opens_scope) {
        symtable_free(&local.variables);
    }



    return error;
}


//provede vsechny casti kontroly
int semantic_check_with_info(const tree_node_t *root, SemanticInfo *info) {
    if(root == NULL) {
        return 99;
    }

    SymTable functions;

    if(!symtable_init(&functions)) {
        return 99;
    }

    size_t main_count = 0;

    int error = semantic_collect_class_names(root, info);
    if(error == 0) {
        error = semantic_collect_type_aliases(root, info);
    }

    if(error == 0) {
        error = semantic_collect_functions(root, &functions, &main_count, info);
    }


    if(error == 0 && main_count == 0) {
        error = 3;
    }
    if(error == 0) {
        error = semantic_check_node(root);
    }
    if(error == 0) {
        error = semantic_check_variables(root, NULL, &functions, info);
    }


    symtable_free(&functions);

    return error;
}


//spusti kontrolu,po dokonceni uvolni pomocne info
int semantic_check(const tree_node_t *root) {
    SemanticInfo info;
    info.items = NULL;
    info.count = 0;
    info.capacity = 0;
    int error = semantic_check_with_info(root, &info);
    semantic_info_free(&info);
    return error;
}