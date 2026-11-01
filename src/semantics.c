
#include "semantics.h"
#include "symtable.h"
#include <stdlib.h>





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


//get function header data
static int semantic_get_function_data(const tree_node_t *node, FuncData *data) {
    
    data->param_types = NULL;
    data->param_count = 0;
    data->return_type = TYPE_UNDEF;

    const tree_node_t *params =node->children[2];
    const tree_node_t *return_type = node->children[3];

    data->param_count = semantic_count_params(params);


    if (data->param_count > 0) {
        
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



static int semantic_check_node(const tree_node_t *node, size_t *main_count) {
    if (node == NULL) {
        return 0;
    }

    int error = 0;

    if (node->type == NODE_NONTERMINAL) {
        
        switch (node->rule) {
            
            case RULE_TOP_ITEM_1: {
                (*main_count)++;

                FuncData data;

                error = semantic_get_function_data(node->children[2], &data);
                if (error != 0) {
                    return error;
                }

                //check main parameters and return type
                if (data.param_count != 0) {
                    error = 5;

                } else {
                    error = semantic_check_main_type(node->children[2]->children[3]);
                }


                //free parameter types
                free(data.param_types);
                if (error != 0) {
                    return error;
                }

                break;
            }
            
            

            default:
                break;
        }
    }



    for (size_t idx = 0; idx < node->children_count; idx++) {
        
        error = semantic_check_node(node->children[idx], main_count);
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

    size_t main_count = 0;

    int error = semantic_check_node(root, &main_count);
    if (error != 0) {
        return error;
    }

    
    if (main_count == 0) {
        return 3;
    }

    if (main_count > 1) {
        return 4;
    }

    return 0;
}