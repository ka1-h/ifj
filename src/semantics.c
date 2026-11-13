
#include "semantics.h"
#include "symtable.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>



const SemanticNodeInfo *semantic_info_find(const SemanticInfo *info, const tree_node_t *node) {
    
    for(size_t idx = 0; idx < info->count; idx++) {
        
        if(info->items[idx].node == node) {
            return &info->items[idx];

        }
    }

    return NULL;
}


void semantic_info_free(SemanticInfo *info) {
   
    free(info->items);
    info->items = NULL;
    info->count = 0;
    info->capacity = 0;
}



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


static DataType semantic_get_type(const tree_node_t *node) {
    
    if(node == NULL || node->type!=NODE_NONTERMINAL) {
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



static DataType semantic_literal_type(const tree_node_t *node) {
    
    if(node==NULL || node->type!=NODE_TERMINAL){
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



//typ uklada k uzlu, aby ho generator nemusel znovu zjistovat
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
    const tree_node_t *owner;
} SemanticScope;



static int semantic_resolve_variable(const tree_node_t *node, const SemanticScope *scope, SemanticInfo *info, DataType *result) {
    const char *name = node->token.data;

    while(scope != NULL) {
        Symbol *symbol = symtable_lookup(&scope->variables, name);

        if(symbol != NULL) {
            
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
                    return 0;
                }
            }


            return 99;
        }

        scope = scope->parent;
    }


    *result = TYPE_UNDEF;
    return 0;
}




static int semantic_type_call(const tree_node_t *node, const SemanticScope *scope, SemanticInfo *info, DataType *result);


static int semantic_type_expression(const tree_node_t *node, const SemanticScope *scope, SemanticInfo *info, DataType *result) {
    *result = TYPE_UNDEF;

    if(node == NULL) {
        return 0;
    }


    if(node->type == NODE_TERMINAL) {
        if(node->token.type == T_ID) {
            return semantic_resolve_variable(node, scope, info, result);
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
    switch(operation) {
        case T_PLUS:
        case T_MINUS:
        case T_MULT:
        case T_DIV:
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
        return 0;
    }

    if(operation==T_PLUS && left_type==TYPE_STRING && right_type==TYPE_STRING) {
        *result = TYPE_STRING;
        return semantic_save_type(info, node, *result);
    }

    if((left_type!=TYPE_INT && left_type!=TYPE_DOUBLE) || (right_type!=TYPE_INT && right_type!=TYPE_DOUBLE )) {
        return 6;
    }


    *result = TYPE_INT;
    if(left_type==TYPE_DOUBLE || right_type==TYPE_DOUBLE) {
        *result = TYPE_DOUBLE;
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



static int semantic_type_call(const tree_node_t *node, const SemanticScope *scope, SemanticInfo *info, DataType *result) {
    
    if(scope == NULL) {
        return 0;
    }

    if(node->children_count != 2) {
        return 0;
    }



    const char *name = node->children[0]->token.data;
    
    Symbol *function = symtable_lookup(scope->functions, name);
    if(function == NULL || function->type != SYM_FUNC) {
        return 0;
    }

    const tree_node_t *definition = NULL;

    for(size_t idx = 0; idx < info->count; idx++) {
        const tree_node_t *candidate = info->items[idx].node;

        if(candidate->rule==RULE_FUNCTION_DEF && strcmp(candidate->children[1]->token.data, name) == 0) {
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
    const tree_node_t *next_list = params->children[params->children_count-1];
    if(next_list->rule != RULE_PARAM_LISTS_2) {
        return 0;
    }

    const tree_node_t *args = node->children[1];
    size_t arg_index = 0;

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


        if(arg_index >= function->func.param_count) {
            return 5;
        }

        const tree_node_t *expression = value->children[0];
        DataType actual;

        int error = semantic_type_expression(expression, scope, info, &actual);
        if(error != 0) {
            return error;
        }

        DataType expected = function->func.param_types[arg_index];
        if(actual==TYPE_UNDEF || expected==TYPE_UNDEF) {
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


    if(arg_index != function->func.param_count) {
        return 5;
    }


    SemanticNodeInfo *item = semantic_info_get_or_add(info, node);
    if(item == NULL) {
        return 99;
    }

    *result = function->func.return_type;
    item->type = *result;
    item->declaration = definition;



    return 0;
}


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


static int semantic_check_param_names(const tree_node_t *node, const char **names, size_t *count) {
    if(node == NULL) {
        return 0;
    }

    if (node->type == NODE_NONTERMINAL &&  node->rule == RULE_PARAM) {
        const char *name = node->children[0]->token.data;

        for (size_t idx = 0; idx < *count; idx++) {
            
            if (strcmp(names[idx], name) == 0) {
                return 4;
            }
        }

        names[*count] = name;
        (*count)++;

        return 0;
    }

    for(size_t idx = 0; idx < node->children_count; idx++) {
        
        int error = semantic_check_param_names(node->children[idx], names, count);


        if(error != 0) {
            return error;
        }
    }


    return 0;
}


static int semantic_get_function_data(const tree_node_t *node, FuncData *data) {
    
    data->param_types = NULL;
    data->param_count = 0;
    data->return_type = TYPE_UNDEF;

    const tree_node_t *params =node->children[2];
    const tree_node_t *return_type = node->children[3];

    data->param_count = semantic_count_params(params);


    if(data->param_count > 0) {
        
        const char **names = malloc(data->param_count * sizeof(*names));

        if (names == NULL) {
            data->param_count = 0;
            return 99;
        }

        size_t count = 0;
        int error = semantic_check_param_names(params, names, &count);



        free(names);
        if(error != 0) {
            return error;
        }
        

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



static int semantic_register_function(const tree_node_t *node, SymTable *functions,SemanticInfo *info) {
    const char *name = node->children[1]->token.data;


    if(symtable_lookup(functions, name) != NULL) {
        return 4;
    }

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

            case RULE_FUNCTION_DEF: {
                
                //u ostatnich funkci jen kontrola headeru
                FuncData data;

                error = semantic_get_function_data(node, &data);
                if (error != 0) {
                    return error;
                }


                free(data.param_types);
                break;
            }



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


static int semantic_check_return(const tree_node_t *node,const tree_node_t *return_type) {
    //return vyzaduje uvedeny navratovy typ
    if(return_type->rule == RULE_OPTIONAL_TYPE_2) {
        return 6;
    }

    const tree_node_t *value = node->children[1];


    //todo - typ hodnoty v returnu
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


//kazda funkce,blok , vlastni tabulka promennych
static int semantic_check_variables(const tree_node_t *node, SemanticScope *scope, const SymTable *functions,SemanticInfo *info) {
    if(node==NULL || node->type!=NODE_NONTERMINAL) {
        return 0;
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
        local.owner = node;
        scope = &local;
    }


    int error = 0;

    if(node->rule == RULE_FUNCTION_DEF) {
        error = semantic_bind_parameters(node->children[2], scope, info);

        //kdyz je telo funkce vyraz, zjisti typ hned po pridani param
        const tree_node_t *rhs = node->children[5];
        if(error == 0 && rhs->rule == RULE_RHS_2) {
            
            const tree_node_t *value = rhs->children[0];

            if(value->rule == RULE_VALUE_2 &&
                value->children[1]->rule == RULE_MATCH_SUFFIX_2) {
                DataType type;
                error = semantic_type_expression(value->children[0], scope, info, &type);
            }
        }

    }else if (node->rule == RULE_VAR_DEFINITION && scope != NULL) {
        error = semantic_check_variable(node, scope, info);
    }

    for(size_t idx = 0; idx < node->children_count && error == 0; idx++) {
        error = semantic_check_variables(node->children[idx], scope, functions, info);
    }

    if (opens_scope) {
        symtable_free(&local.variables);
    }



    return error;
}


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


int semantic_check(const tree_node_t *root) {
    SemanticInfo info;
    info.items = NULL;
    info.count = 0;
    info.capacity = 0;
    int error = semantic_check_with_info(root, &info);
    semantic_info_free(&info);
    return error;
}