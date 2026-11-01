#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../src/semantics.c"

typedef struct {
    tree_node_t nodes[100];
    tree_node_t *children[100][6];
    char names[100][16];
    size_t count;
} TestTree;

static tree_node_t *test_node(TestTree *tree, tree_rule_t rule) {
    assert(tree->count < 100);

    size_t idx = tree->count++;
    tree_node_t *node = &tree->nodes[idx];

    node->type = NODE_NONTERMINAL;
    node->rule = rule;
    node->children = tree->children[idx];

    return node;
}

static void test_child(tree_node_t *parent, tree_node_t *child) {
    assert(parent->children_count < 6);

    parent->children[parent->children_count++] = child;
    child->parent = parent;
}

static tree_node_t *test_terminal(TestTree *tree) {
    tree_node_t *node = test_node(tree, TREE_RULE_NONE);
    size_t idx = tree->count - 1;

    node->type = NODE_TERMINAL;

    snprintf(tree->names[idx], sizeof(tree->names[idx]), "id_%zu", idx);
    node->token.data = tree->names[idx];

    return node;
}

static tree_node_t *test_type(TestTree *tree, tree_rule_t rule) {
    tree_node_t *type = test_node(tree, RULE_TYPE_1);
    tree_node_t *simple = test_node(tree, rule);

    test_child(simple, test_terminal(tree));
    test_child(type, simple);
    test_child(type, test_node(tree, RULE_UNION_REST_2));

    return type;
}

static tree_node_t *test_params(TestTree *tree,
                               const tree_rule_t *types,
                               size_t count) {
    tree_node_t *lists = test_node(tree, RULE_PARAM_LISTS_1);

    if (count == 0) {
        test_child(lists, test_terminal(tree));
        return lists;
    }

    test_child(lists, test_terminal(tree));

    tree_node_t *list = test_node(tree, RULE_PARAM_LIST_1);
    test_child(lists, list);

    for (size_t idx = 0; idx < count; idx++) {
        tree_node_t *param = test_node(tree, RULE_PARAM);

        test_child(param, test_terminal(tree));
        test_child(param, test_terminal(tree));
        test_child(param, test_type(tree, types[idx]));
        test_child(list, param);

        if (idx + 1 < count) {
            tree_node_t *rest = test_node(tree, RULE_PARAM_LIST_REST_1);
            test_child(list, rest);
            test_child(rest, test_terminal(tree));

            list = test_node(tree, RULE_PARAM_LIST_1);
            test_child(rest, list);
        } else {
            test_child(list, test_node(tree, RULE_PARAM_LIST_REST_2));
        }
    }

    test_child(lists, test_terminal(tree));
    return lists;
}

static void test_header(const char *name,
                        const tree_rule_t *types,
                        const DataType *expected,
                        size_t first_count,
                        size_t second_count,
                        tree_rule_t return_rule,
                        DataType expected_return) {
    TestTree tree = {0};
    tree_node_t *function = test_node(&tree, RULE_FUNCTION_DEF);

    test_child(function, test_terminal(&tree));
    test_child(function, test_terminal(&tree));

    tree_node_t *params = test_params(&tree, types, first_count);
    test_child(function, params);

    if (second_count > 0) {
        tree_node_t *second = test_params(
            &tree, types + first_count, second_count
        );
        test_child(params, second);
        params = second;
    }

    test_child(params, test_node(&tree, RULE_PARAM_LISTS_2));

    tree_node_t *optional;

    if (return_rule == TREE_RULE_NONE) {
        optional = test_node(&tree, RULE_OPTIONAL_TYPE_2);
    } else {
        optional = test_node(&tree, RULE_OPTIONAL_TYPE_1);
        test_child(optional, test_terminal(&tree));
        test_child(optional, test_type(&tree, return_rule));
    }

    test_child(function, optional);
    test_child(function, test_terminal(&tree));
    test_child(function, test_node(&tree, RULE_RHS_2));

    FuncData data;
    assert(semantic_get_function_data(function, &data) == 0);

    size_t count = first_count + second_count;
    assert(data.param_count == count);
    assert(data.return_type == expected_return);

    if (count == 0) {
        assert(data.param_types == NULL);
    } else {
        assert(data.param_types != NULL);

        for (size_t idx = 0; idx < count; idx++) {
            assert(data.param_types[idx] == expected[idx]);
        }
    }

    free(data.param_types);
    printf("[PASS] %s\n", name);
}

static void test_returns(void) {
    TestTree tree = {0};

    tree_node_t *optional = test_node(&tree, RULE_OPTIONAL_TYPE_1);
    test_child(optional, test_terminal(&tree));

    tree_node_t *type = test_type(&tree, RULE_SIMPLE_TYPE_4);
    test_child(optional, type);

    tree_node_t *statement = test_node(&tree, RULE_RETURN_STATEMENT);
    test_child(statement, test_terminal(&tree));

    tree_node_t *value = test_node(&tree, RULE_RETURN_VALUE_2);
    test_child(statement, value);

    assert(semantic_check_return(statement, optional) == 0);
    puts("[PASS] 08_bare_return_unit");

    type->children[0]->rule = RULE_SIMPLE_TYPE_1;
    assert(semantic_check_return(statement, optional) == 6);
    puts("[PASS] 09_bare_return_int");

    optional->rule = RULE_OPTIONAL_TYPE_2;
    assert(semantic_check_return(statement, optional) == 6);
    puts("[PASS] 10_bare_return_missing_type");

    value->rule = RULE_RETURN_VALUE_1;
    assert(semantic_check_return(statement, optional) == 6);
    puts("[PASS] 11_value_return_missing_type");

    optional->rule = RULE_OPTIONAL_TYPE_1;
    type->children[0]->rule = RULE_SIMPLE_TYPE_4;
    test_child(value, test_node(&tree, RULE_VALUE_2));

    assert(semantic_check_return(statement, optional) == 0);
    puts("[PASS] 12_value_return_unit");

    tree_node_t *body = test_node(&tree, RULE_BLOCK);
    tree_node_t *branch = test_node(&tree, RULE_IF_STATEMENT);
    tree_node_t *loop = test_node(&tree, RULE_WHILE_STATEMENT);

    test_child(body, branch);
    test_child(branch, loop);
    test_child(loop, statement);

    optional->rule = RULE_OPTIONAL_TYPE_2;
    assert(semantic_check_returns(body, optional) == 6);
    puts("[PASS] 13_nested_return_missing_type");

    optional->rule = RULE_OPTIONAL_TYPE_1;
    assert(semantic_check_returns(body, optional) == 0);
    puts("[PASS] 14_nested_return_unit");
}

int main(void) {
    tree_rule_t types[] = {
        RULE_SIMPLE_TYPE_1,
        RULE_SIMPLE_TYPE_2,
        RULE_SIMPLE_TYPE_3,
        RULE_SIMPLE_TYPE_4,
        RULE_SIMPLE_TYPE_5
    };

    DataType expected[] = {
        TYPE_INT,
        TYPE_DOUBLE,
        TYPE_STRING,
        TYPE_UNIT,
        TYPE_BOOL
    };

    test_header("01_empty_params", NULL, NULL, 0, 0,
                RULE_SIMPLE_TYPE_4, TYPE_UNIT);

    test_header("02_one_param", types, expected, 1, 0,
                RULE_SIMPLE_TYPE_3, TYPE_STRING);

    test_header("03_param_order", types, expected, 5, 0,
                RULE_SIMPLE_TYPE_1, TYPE_INT);

    test_header("04_multiple_param_lists", types, expected, 2, 3,
                RULE_SIMPLE_TYPE_2, TYPE_DOUBLE);

    test_header("05_missing_return_type", types, expected, 2, 0,
                TREE_RULE_NONE, TYPE_UNDEF);

    test_header("06_empty_params_missing_type", NULL, NULL, 0, 0,
                TREE_RULE_NONE, TYPE_UNDEF);

    test_header("07_boolean_return_type", NULL, NULL, 0, 0,
                RULE_SIMPLE_TYPE_5, TYPE_BOOL);

    test_returns();
    puts("All semantic unit tests passed");
    return 0;
}