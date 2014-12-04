#ifndef BSTREE_H
#define BSTREE_H

#include <stdbool.h>
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

/**
 * Link a node to a parent node.
 */
static inline void bstree_link_node(struct bstree_node *node,
                                   struct bstree_node *parent,
                                   struct bstree_node **target)
{
    node->parent = parent;
    *target = node;
}

/**
 * Test wether a tree is empty.
 */
static inline bool bstree_is_empty(struct bstree_node *tree)
{
    return !tree;
}

/**
 * Delete a node.
 */
static inline bool bstree_delete(struct bstree_node **tree,
                                 struct bstree_node *node)
{
    if (!*tree)
        return false;

    struct bstree_node **parent_link = node->parent ?
        &(node->parent->link[node->parent->link[RIGHT] == node]) :
        tree;

    if (node->link[LEFT] && node->link[RIGHT]) { // delete by copy
        struct bstree_node *succ = node->link[RIGHT];
        while (succ->link[LEFT]) {
            succ = succ->link[LEFT];
        }

        struct bstree_node *succ_parent = succ->parent;
        succ_parent->link[succ_parent->link[RIGHT] == succ] = succ->link[RIGHT];

        succ->link[LEFT] = node->link[LEFT];
        succ->link[RIGHT] = node->link[RIGHT];
        succ->parent = node->parent;

        *parent_link = succ;
    }
    /*
        p          p
         \          \
          x    ->    ~
         / \
        ~   ~
     */
    else if (!node->link[LEFT] && !node->link[RIGHT]) {
        *parent_link = NULL;
    }
    /*
        p           p             p           p
         \           \             \           \
          x     ->    c    OR       x    ->     c
         / \         / \           / \         / \
        ~   c       *   *         c   ~       *   *
           / \                   / \
          *   *                 *   *
     */
    else {
        int child_dir = node->link[LEFT] == NULL;
        bstree_link_node(node->link[child_dir], node->parent, parent_link);
    }

    return true;
}

static inline struct bstree_node *__bstree_end(struct bstree_node *tree, int dir)
{
    if (!tree) return NULL;

    /* Make sure we're on the root. */
    while (tree->parent)
        tree = tree->parent;

    while (tree->link[dir])
        tree = tree->link[dir];
    return tree;
}

/**
 * Find the first, lowest node.
 *
 */
static inline struct bstree_node *bstree_first(struct bstree_node *tree)
{
    return __bstree_end(tree, LEFT);
}

/**
 * Find the last, highest node.
 */
static inline struct bstree_node *bstree_last(struct bstree_node *tree)
{
    return __bstree_end(tree, RIGHT);
}

/**
 * Find the next/previous inorder node.
 */
static inline struct bstree_node*
__bstree_iterate(const struct bstree_node *node, int dir)
{
    if (!node) return NULL;

    int opp = dir ^ RIGHT;
    dir = opp ^ RIGHT; // just make sure dir is RIGHT or LEFT

    struct bstree_node *it = (struct bstree_node *)node;
    // last opp child after dir child
    if ((it = node->link[dir])) {
        while (it->link[opp])
            it = it->link[opp];
    }
    // first dir parent after last opp parent, or null
    else {
        while ((it = node->parent) && it->link[dir^RIGHT] != node)
            node = it;
    }
    return it;
}

/**
 * Find the next inorder node.
 */
static inline struct bstree_node *bstree_next(const struct bstree_node *node)
{
    return __bstree_iterate(node, RIGHT);
}

/**
 * Find the previous inorder node.
 */
static inline struct bstree_node *bstree_prev(const struct bstree_node *node)
{
    return __bstree_iterate(node, LEFT);
}

#endif /* BSTREE_H */
