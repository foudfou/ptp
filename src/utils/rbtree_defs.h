#ifndef RBTREE_DEFS_H
#define RBTREE_DEFS_H

#include <stddef.h>

struct rbtree_node {
    int color;
#define RB_RED   0
#define RB_BLACK 1
    struct rbtree_node * parent;
    struct rbtree_node * link[2];
#define RB_LEFT  0
#define RB_RIGHT 1
};

/**
 * Declare a btree.
 */
#define RBTREE_DECL(tree) struct rbtree_node *tree = NULL

/**
 * Init a btree node.
 *
 * The root node has parent NULL.
 */
#define RBTREE_NODE_INIT(node) (node).color = RB_RED; (node).parent = NULL; \
    (node).link[RB_LEFT] = NULL; (node).link[RB_RIGHT] = NULL


#endif /* RBTREE_DEFS_H */