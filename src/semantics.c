
#include "semantics.h"
#include "symtable.h"
#include <stdlib.h>
#include <string.h>





//count parameters
static size_t semantic_count_params(const tree_node_t *node) {
    
    if (node == NULL) {
        return 0;
    }

    if (node->type == NODE_NONTERMINAL && node->rule == RULE_PARAM) {
        return 1;
    }

    size_t count = 0;

    for (size_t idx = 0; idx < node->children_count; idx++) {
        count += semantic_count_params(node->children[idx]);
    }


    
    return count;
}


//get type from type node
static DataType semantic_get_type(const tree_node_t *node) {
    
    if (node == NULL || node->type!=NODE_NONTERMINAL) {
        return TYPE_UNDEF;
    }

    
    if (node->rule != RULE_TYPE_1) {
        return TYPE_UNDEF;
    }


    if (node->children[1]->rule == RULE_UNION_REST_1) {
        return TYPE_UNDEF;
    }


    const tree_node_t *simple_type = node->children[0];

    switch (simple_type->rule) {
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



//save parameter types in order
static void semantic_get_param_types(const tree_node_t *node, DataType *param_types, size_t *position) {
    if (node == NULL) {
        return;
    }

    if (node->type == NODE_NONTERMINAL && node->rule == RULE_PARAM) {
        
        param_types[*position] = semantic_get_type(node->children[2]);
        (*position)++;
        
        
        return;
    }

    for (size_t idx = 0; idx < node->children_count; idx++) {
        semantic_get_param_types(node->children[idx], param_types, position);

    }
}


//check return type of main
static int semantic_check_main_type(const tree_node_t *node) {
    
    
    if (node->rule == RULE_OPTIONAL_TYPE_2) {
        return 0;
    }

    const tree_node_t *type_node = node->children[1];

    //function type cant be unit
    if (type_node->rule == RULE_TYPE_2) {
        return 6;
    }

    DataType type = semantic_get_type(type_node);

    
    if (type == TYPE_UNDEF) {
        return 0;
    }

    if (type != TYPE_UNIT) {
        return 6;
    }

    return 0;
}


//check duplicate parameter names
static int semantic_check_param_names(const tree_node_t *node, const char **names, size_t *count) {
    if (node == NULL) {
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

    for (size_t idx = 0; idx < node->children_count; idx++) {
        int error = semantic_check_param_names(
            node->children[idx], names, count
        );

        if (error != 0) {
            return error;
        }
    }


    return 0;
}


//get function header data
static int semantic_get_function_data(const tree_node_t *node, FuncData *data) {
    
    data->param_types = NULL;
    data->param_count = 0;
    data->return_type = TYPE_UNDEF;

    const tree_node_t *params =node->children[2];
    const tree_node_t *return_type = node->children[3];

    data->param_count = semantic_count_params(params);

    //get parameter data
    if (data->param_count > 0) {
        
        //check parameter names
        const char **names = malloc(data->param_count * sizeof(*names));

        if (names == NULL) {
            data->param_count = 0;
            return 99;
        }

        size_t count = 0;
        int error = semantic_check_param_names(params, names, &count);


        free(names);
        if (error != 0) {
            return error;
        }
        

        //get parameter types
        data->param_types = malloc(data->param_count * sizeof(*data->param_types ));
        if (data->param_types == NULL) {
            data->param_count = 0;
            return 99;
        }


        size_t position = 0;
        semantic_get_param_types(params, data->param_types, &position);
    }

    //get declared return type
    if (return_type->rule == RULE_OPTIONAL_TYPE_1) {
        data->return_type = semantic_get_type(return_type->children[1]);
    }

    return 0;
}



//add function to global symtable
static int semantic_register_function(const tree_node_t *node, SymTable *functions) {
    const char *name = node->children[1]->token.data;

    //check duplicate function
    if (symtable_lookup(functions, name) != NULL) {
        return 4;
    }

    FuncData data;

    int error = semantic_get_function_data(node, &data);
    if (error != 0) {
        return error;
    }

    int result = symtable_insert(functions, name);

    if (result != RET_OK) {
        free(data.param_types);

        if (result == RET_DUPLICATE) {
            return 4;
        }

        return 99;
    }


    
    Symbol *symbol = symtable_lookup(functions, name);
    if (symbol == NULL) {
        free(data.param_types);
        return 99;
    }

    symbol->type = SYM_FUNC;
    symbol->func = data;

    return 0;
}



//collect global functions and check function headers
static int semantic_collect_functions(const tree_node_t *node, SymTable *functions, size_t *main_count) {
    
    if (node == NULL) {
        return 0;
    }

    int error = 0;

    if (node->type == NODE_NONTERMINAL) {
        switch (node->rule) {
            case RULE_TOP_ITEM_1:
                (*main_count)++;

                if (*main_count > 1) {
                    return 4;
                }

                return semantic_register_function(
                    node->children[2], functions
                );

            case RULE_TOP_ITEM_2:
                return semantic_register_function(
                    node->children[0], functions
                );

            case RULE_FUNCTION_DEF: {
                
                //check other function headers without global registration
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
       
        error = semantic_collect_functions(node->children[idx], functions, main_count);
        if (error != 0) {
            return error;
        }
    }

    return 0;
}


//check one return
static int semantic_check_return(const tree_node_t *node,const tree_node_t *return_type) {
    //return requires declared type
    if (return_type->rule == RULE_OPTIONAL_TYPE_2) {
        return 6;
    }

    const tree_node_t *value = node->children[1];

    //check return with value later
    if (value->rule == RULE_RETURN_VALUE_1) {
        return 0;
    }

    const tree_node_t *type_node = return_type->children[1];

    if (type_node->rule == RULE_TYPE_2) {
        return 6;
    }

    DataType type = semantic_get_type(type_node);

    //resolve aliases and union types later
    if (type == TYPE_UNDEF) {
        return 0;
    }

    //bare return requires Unit
    if (type != TYPE_UNIT) {
        return 6;
    }

    return 0;
}


//check returns in one function body
static int semantic_check_returns(const tree_node_t *node,const tree_node_t *return_type) {
    
    if (node == NULL) {
        return 0;
    }

    int error = 0;

    if (node->type == NODE_NONTERMINAL) {
        
        //another function has its own return type
        if (node->rule == RULE_FUNCTION_DEF) {
            return 0;
        }

        if (node->rule == RULE_RETURN_STATEMENT) {
            error = semantic_check_return(node, return_type);

            if (error != 0) {
                return error;
            }
        }
    }

    for (size_t idx = 0; idx < node->children_count; idx++) {
        error = semantic_check_returns(
            node->children[idx], return_type
        );

        if (error != 0) {
            return error;
        }
    }

    return 0;
}


//check main and function bodies
static int semantic_check_node(const tree_node_t *node) {
    if (node == NULL) {
        return 0;
    }

    int error = 0;

    if (node->type == NODE_NONTERMINAL) {
        switch (node->rule) {
            case RULE_TOP_ITEM_1:
                
            //main cant have parameters
                if (semantic_count_params(node->children[2]->children[2]) != 0) {
                    return 5;
                }

                error = semantic_check_main_type(node->children[2]->children[3]);

                if (error != 0) {
                    return error;
                }

                break;

            case RULE_FUNCTION_DEF:
                error = semantic_check_returns(node->children[5], node->children[3]);
                if (error != 0) {
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


int semantic_check(const tree_node_t *root) {
    if (root == NULL) {
        return 99;
    }

    SymTable functions;

    if (!symtable_init(&functions)) {
        return 99;
    }

    size_t main_count = 0;

    int error = semantic_collect_functions(
        root, &functions, &main_count
    );


    if (error == 0 && main_count == 0) {
        error = 3;
    }

    if (error == 0) {
        error = semantic_check_node(root);
    }





    symtable_free(&functions);

    return error;
}