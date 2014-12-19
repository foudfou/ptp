#ifndef RBTREE_H
#define RBTREE_H

/**
 * A red-black binary tree.
 *
 * Better than hash if order matters.
 * Inspired by the Linux kernel and Julienne Walker.
 *
 * This header can be imported multiple times to create different bstree types.
 * Just make sure to use a different RBTREE_NAME.
 *
 * Consumer MUST provide a key comparison function:
 *
 * int RBTREE_FUNCTION(compare)(RBTREE_KEY_TYPE keyA, RBTREE_KEY_TYPE keyB);
 */

#include <stdbool.h>
#include "base.h"
#include "rbtree_defs.h"
/* #include "cont.h" */

int is_red(struct rbtree_node *root)
{
   return root != NULL && root->color == RB_RED;
}

/**
 * Test wether a tree is empty.
 */
static inline bool rbtree_is_empty(struct rbtree_node *tree)
{
    return !tree;
}

/**
 * Link a node to a parent node.
 *
 * @parent assumed to be non-NULL (is not the tree itself) !
 */
static inline void rbtree_link_node(struct rbtree_node *parent,
                                    struct rbtree_node *node, int dir)
{
    node->parent = parent;
    parent->link[dir] = node;
}

/**
 * Rotate at @root in direction @dir.
 *
 *     x             y
 *    / \           / \
 *   a   y   <->   x   c
 *      / \       / \
 *     b   c     a   b
 */
struct rbtree_node *rbtree_rotate(struct rbtree_node *root, int dir)
{
    struct rbtree_node *new = root->link[!dir];

    if (new->link[dir])
        new->link[dir]->parent = root;
    root->link[!dir] = new->link[dir];

    new->parent = root->parent;

    root->parent = new;
    new->link[dir] = root;

    return new;
}

/**
 * Double rotation
 *
 * Rotate at in one direction at child of @root, then in the other direction
 * @dir at @root.
 *
 *       z                z             y
 *      / \              / \          /   \
 *     x   d            y   d        x     z
 *    / \       ->     / \     ->   / \   / \
 *   a   y            x   c        a   b c   d
 *      / \          / \
 *     b   c        a   b
 */
struct rbtree_node *rbtree_rotate_double(struct rbtree_node *root, int dir)
{
    root->link[!dir] = rbtree_rotate(root->link[!dir], !dir);
    return rbtree_rotate(root, dir);
}

#endif /* RBTREE_H */
