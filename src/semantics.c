
#include "semantics.h"
#include "symtable.h"



//check that main has no parameter
static int semantic_check_main_params(const tree_node_t *node) {
    if (node == NULL) {
        return 0;
    }


    if (node->type == NODE_NONTERMINAL && node->rule == RULE_PARAM) {
        return 5;
    }

    for (size_t idx = 0; idx < node->children_count; idx++) {
        int error = semantic_check_main_params(node->children[idx]);

        if (error != 0) {
            return error;
        }
    }

    return 0;
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


static int semantic_check_node(const tree_node_t *node, size_t *main_count) {
    if (node == NULL) {
        return 0;
    }

    int error = 0;

    if (node->type == NODE_NONTERMINAL) {
        
        switch (node->rule) {
            
            case RULE_TOP_ITEM_1:
                (*main_count)++;

                error = semantic_check_main_params(node->children[2]->children[2]);
                if (error != 0) {
                    return error;
                }

                break;
            


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