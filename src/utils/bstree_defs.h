#ifndef BSTREE_DEFS_H
#define BSTREE_DEFS_H

#include <stddef.h>

/**
 * A simple, non-balanced, binary search tree.
 *
 * Inspired by the Linux kernel and Julienne Walker.
 */

struct bstree_node {
    struct bstree_node * parent;
    struct bstree_node * link[2];
#define LEFT  0
#define RIGHT 1
};

/**
 * Declare and init a btree.
 */
#define BSTREE_DECL(tree) struct bstree_node *tree = NULL
#define BSTREE_NODE_INIT(node) (node).parent = NULL;     \
    (node).link[LEFT] = NULL; (node).link[RIGHT] = NULL


#endif /* BSTREE_DEFS_H */
