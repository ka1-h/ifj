#ifndef GENERATOR_H
#define GENERATOR_H

#include <stdio.h>

#include "semantics.h"
#include "tree.h"

// writes the IFJcode26 program for the whole tree to out. must be called only after a successful semantic check. returns 0 on sucess, 99 on internal error.
int generate_code(const tree_node_t *root, const SemanticInfo *info, FILE *out);

#endif