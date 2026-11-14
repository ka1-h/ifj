#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "tree.h"

typedef struct {
    const tree_node_t *node;
    DataType type;
    const tree_node_t *declaration;
    bool promote_to_double;
    const tree_node_t *scope_owner;
} SemanticNodeInfo;

typedef struct {
    SemanticNodeInfo *items;
    size_t count;
    size_t capacity;
} SemanticInfo;

const SemanticNodeInfo *semantic_info_find(const SemanticInfo *info, const tree_node_t *node);


void semantic_info_free(SemanticInfo *info);

int semantic_check(const tree_node_t *root);

int semantic_check_with_info(const tree_node_t *root, SemanticInfo *info);

#endif