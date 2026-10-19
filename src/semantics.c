
#include "semantics.h"



static int semantic_check_node(const tree_node_t *node, size_t *main_count) {
    if (node == NULL) {
        return 0;
    }

    int error = 0;

    if (node->type == NODE_NONTERMINAL) {
        
        switch (node->rule) {
            case RULE_TOP_ITEM_1:
                (*main_count)++;
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