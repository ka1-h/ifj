#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "syntactic.h"
#include "tree.h"

void rule_expression(Parser *p, tree_node_t *parent);
void rule_expression_after_id(Parser *p, tree_node_t *parent, tree_node_t *id_leaf);

#endif
