
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
    item->declaration = NULL;
    item->scope_owner = NULL;
    item->promote_to_double = false ;

    info->count++;



    return item;
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
static int semantic_save_type(SemanticInfo *info, const tree_node_t *node,DataType type) {
    
    SemanticNodeInfo *item = semantic_info_get_or_add(info, node);
    if(item == NULL) {
        return 99;
    }


    item->type = type;
    return 0;
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
                }else if (declaration->rule == RULE_PARAM) {
                    declared_name = declaration->children[0];
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

                    return 0;
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


static int semantic_type_call(const tree_node_t *node, const SemanticScope *scope, SemanticInfo *info, DataType *result);


//zjisti typ vyrazu, overi kompatibilitu operandu
static int semantic_type_expression(const tree_node_t *node, const SemanticScope *scope, SemanticInfo *info, DataType *result) {
    *result = TYPE_UNDEF;

    if(node == NULL) {
        return 0;
    }


    if(node->type == NODE_TERMINAL) {
        if(node->token.type == T_ID) {
            return semantic_resolve_variable(node, scope, info, result, NULL);
        }
                
        
        *result = semantic_literal_type(node);
        if(*result == TYPE_UNDEF) {
            return 0;
        }

        return semantic_save_type(info, node, *result);
    }


    if(node->rule == RULE_EXPRESSION_AFTER_ID) {
        return semantic_type_call(node, scope, info, result);
    }


    if(node->rule == RULE_EXPRESSION_UNARY) {
        const tree_node_t *operation = node->children[0];
        const tree_node_t *operand = node->children[1];

        if(operation->token.type != T_MINUS) {
            return 0;
        }

        int error = semantic_type_expression(operand, scope, info, result);
        if(error != 0) {
            return error;
        }

        if(*result == TYPE_UNDEF) {
            return 0;
        }

        
        if(*result != TYPE_INT && *result != TYPE_DOUBLE) {
            return 6;
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


        if(*result == TYPE_UNDEF) {
            return 0;
        }

        return semantic_save_type(info, node, *result);
    }

    if (node->children_count != 3) {
        return 0;
    }


    t_type operation = node->children[1]->token.type;
    bool is_comparison = false;
    bool is_equality = false;

    switch(operation) {
        case T_PLUS:
        case T_MINUS:
        case T_MULT:
        case T_DIV:
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


    if(left_type==TYPE_BOOL || right_type==TYPE_BOOL) {
        return 6;
    }

    if(left_type==TYPE_UNDEF || right_type==TYPE_UNDEF) {
        return 0;
    }

    if(operation==T_PLUS && left_type==TYPE_STRING && right_type==TYPE_STRING) {
        *result = TYPE_STRING;
        return semantic_save_type(info, node, *result);
    }

    if(is_equality && left_type == right_type) {
        *result = TYPE_BOOL;
        return semantic_save_type(info, node, *result);
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
static int semantic_type_call(const tree_node_t *node, const SemanticScope *scope, SemanticInfo *info, DataType *result) {
    
    if(scope == NULL) {
        return 0;
    }

    if(node->children_count != 2) {
        return 0;
    }



        const char *name = node->children[0]->token.data;
        const tree_node_t *definition = NULL;
        const DataType *param_types;
        size_t param_count;
        DataType return_type;
        bool accepts_any_type = false;

        BuiltinFunction builtin;
        Symbol *function = symtable_lookup(scope->functions, name);

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

                if(candidate->rule == RULE_FUNCTION_DEF &&
                    strcmp(candidate->children[1]->token.data, name) == 0) {
                    definition = candidate;
                    break;
                }
            }

            if(definition == NULL) {
                return 99;
            }

            const tree_node_t *params = definition->children[2];
            if(params->rule != RULE_PARAM_LISTS_1) {
                return 0;
            }

            const tree_node_t *next_list = params->children[params->children_count - 1];
            if(next_list->rule != RULE_PARAM_LISTS_2) {
                return 0;
            }

            param_types = function->func.param_types;
            param_count = function->func.param_count;
            return_type = function->func.return_type;
        }

    size_t arg_index = 0;
    const tree_node_t *args = node->children[1];

    for(size_t idx = 0; idx < args->children_count; idx++) {
    
        const tree_node_t *value = args->children[idx];

        if(value->type == NODE_TERMINAL) {
            continue;
        }

        if(value->rule != RULE_VALUE_2) {
            return 0;
        }
        if(value->children[1]->rule != RULE_MATCH_SUFFIX_2) {
            return 0;
        }


        if(arg_index >= param_count) {
            return 5;
        }

        const tree_node_t *expression = value->children[0];
        DataType actual;

        int error = semantic_type_expression(expression, scope, info, &actual);
        if(error != 0) {
            return error;
        }


        if(actual == TYPE_BOOL) {
            return 5;
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
static int semantic_register_function(const tree_node_t *node, SymTable *functions,SemanticInfo *info) {
    const char *name = node->children[1]->token.data;

    FuncData data;

    int error = semantic_get_function_data(node, &data);
    if(error != 0) {
        return error;
    }

    int result = symtable_insert(functions, name);
    if(result != RET_OK) {
        free(data.param_types);
        if(result == RET_DUPLICATE) {
            return 4;
        }

        return 99;
    }


    Symbol *symbol = symtable_lookup(functions, name);
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

    //return bez hodnoty ma byt unit
    if(type != TYPE_UNIT) {
        return 6;
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
    if(value->rule != RULE_VALUE_2) {
        return 0;
    }
    if(value->children[1]->rule != RULE_MATCH_SUFFIX_2) {
        return 0;
    }

    const tree_node_t *expression = value->children[0];
    DataType type;
    int error = semantic_type_expression(expression, scope, info, &type);

    if(error != 0) {
        return error;
    }

    if(type == TYPE_UNDEF) {
        return 0;
    }
    if(type == TYPE_BOOL) {
        return 6;
    }


    if(annotation->rule == RULE_OPTIONAL_TYPE_1) {
        DataType declared_type = semantic_get_type(annotation->children[1]);

        if(declared_type == TYPE_UNDEF) {
            return 0;
        }

        if(declared_type == TYPE_DOUBLE && type == TYPE_INT) {
            SemanticNodeInfo *initializer = semantic_info_get_or_add(info, expression);
            if (initializer == NULL) {
                return 99;
            }


            initializer->promote_to_double = true;
        }else if (declared_type != type) {
            return 6;
        }

        type = declared_type;
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
    if(value->rule != RULE_VALUE_2) {
        return 0;
    }
    if(value->children[1]->rule != RULE_MATCH_SUFFIX_2) {
        return 0;
    }

    const tree_node_t *expression = value->children[0];
    DataType value_type;
    
    error = semantic_type_expression(expression, scope, info, &value_type);
    if(error != 0) {
        return error;
    }
    
    if(value_type == TYPE_UNDEF || variable_type == TYPE_UNDEF) {
        return 0;
    }
    if(value_type == TYPE_BOOL) {
        return 6;
    }

    if(variable_type == value_type) {
        return 0;
    }


    if(variable_type == TYPE_DOUBLE && value_type == TYPE_INT) {
        SemanticNodeInfo *item = semantic_info_get_or_add(info, expression);
        if(item == NULL) {
            return 99;
        }

        item->promote_to_double = true;
        return 0;
    }



    return 6;
}


//prida parametr funkce mezi promenne
static int semantic_bind_parameters(const tree_node_t *node, SemanticScope *scope, SemanticInfo *info) {
    if(node->type != NODE_NONTERMINAL) {
        return 0;
    }

    if(node->rule == RULE_PARAM) {
        const char *name = node->children[0]->token.data;
        DataType type = semantic_get_type(node->children[2]);

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

    if(type == TYPE_BOOL) {
        return 6;
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
    if(value->rule != RULE_VALUE_2) {
        return 0;
    }

    if(value->children[1]->rule != RULE_MATCH_SUFFIX_2) {
        return 0;
    }


    const tree_node_t *expression = value->children[0];
    DataType expected_type = semantic_get_type(return_type->children[1]);
    DataType actual_type;

    int error = semantic_type_expression(expression, return_scope, info, &actual_type);
    if(error != 0) {
        return error;
    }


    if(actual_type == TYPE_BOOL) {
        return 6;
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
    if(value->rule != RULE_VALUE_2) {
        return 0;
    }
    if(value->children[1]->rule != RULE_MATCH_SUFFIX_2) {
        return 0;
    }

    const tree_node_t *expression = value->children[0];
    DataType actual_type;

    int error = semantic_type_expression(expression, scope, info, &actual_type);
    if(error != 0) {
        return error;
    }

    if(actual_type == TYPE_BOOL) {
        return 6;
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

        if(else_part->rule != RULE_ELSE_PART_1) {
            return 6;
        }

        const tree_node_t *else_body = else_part->children[1];
        if(then_body->rule!=RULE_BODY_1 || else_body->rule!=RULE_BODY_1) {
            return 6;
        }

        const tree_node_t *branch_expression;
        DataType then_type;
        DataType else_type;
        bool then_returns;
        bool else_returns;

        int error = semantic_get_body_result(then_body->children[0], info, expected_type, &branch_expression, &then_type, &then_returns);
        if(error != 0) {
            return error;
        }

        error = semantic_get_body_result(else_body->children[0], info, expected_type, &branch_expression, &else_type, &else_returns);
        if(error != 0) {
            return error;
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
    if(node->rule == RULE_ID_STATEMENT_2 || node->rule == RULE_INLINE_STATEMENT_6) {
        
        *expression = node->children[0];

        const SemanticNodeInfo *item = semantic_info_find(info, *expression);
        if(item==NULL || item->type==TYPE_UNDEF) {
            return 6;
        }

        *type = item->type;
    }

    return 0;
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
        if(value->rule != RULE_VALUE_2) {
            return 6;
        }
        if(value->children[1]->rule != RULE_MATCH_SUFFIX_2) {
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
    if(actual_type == TYPE_BOOL) {
        return 6;
    }

    if(annotation->rule == RULE_OPTIONAL_TYPE_2) {
        if(function->parent->rule==RULE_TOP_ITEM_1 && actual_type!=TYPE_UNIT) {
            return 6;
        }

        int error = semantic_save_type(info, function, actual_type);
        if(error != 0) {
            return error;
        }

        const char *name = function->children[1]->token.data;
        Symbol *symbol = symtable_lookup(functions, name);
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



//kazda funkce/blok ma vlastni tabulku promennych
static int semantic_check_variables(const tree_node_t *node, SemanticScope *scope, const SymTable *functions,SemanticInfo *info) {
    if(node == NULL) {
        return 0;
    }
    if(node->type != NODE_NONTERMINAL) {
        return 0;
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
        error = semantic_bind_parameters(node->children[2], scope, info);
        if(error == 0) {
            error = semantic_check_inline_body(node, scope, info);
        }

    }else if (node->rule==RULE_VAR_DEFINITION && scope!=NULL) {
        error = semantic_check_variable(node, scope, info);

    }else if (node->rule==RULE_ID_STATEMENT_1 && scope!=NULL) {
        error = semantic_check_assignment(node, scope, info);

    }else if (node->rule==RULE_IF_STATEMENT || node->rule==RULE_WHILE_STATEMENT) {
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

    int error = semantic_collect_functions(root, &functions, &main_count, info);


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