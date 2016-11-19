#ifndef RBTREE_H
#define RBTREE_H

/**
 * A red-black binary tree.
 *
 * Better than hash if order matters.
 * Inspired by the Linux kernel and Julienne Walker.
 *
 * This header can be imported multiple times with RBTREE_CREATE to create
 * different rbtree types. Just make sure to use a different RBTREE_NAME.
 *
 * Consumer MUST provide a key comparison function:
 *
 * int RBTREE_FUNCTION(compare)(RBTREE_KEY_TYPE keyA, RBTREE_KEY_TYPE keyB);
 */
#include "btree_def.h"

/**
 * Red-black tree rules:
 * 1) Every node has a color either red or black.
 * 2) Root of tree is always black.
 * 3) There are no two adjacent red nodes (A red node cannot have a red parent
 *    or red child).
 * 4) Every path from root to a NULL node has same number of black nodes.
 */
struct rbtree_node {
    int color;
#define RB_RED   0
#define RB_BLACK 1
    BTREE_MEMBERS(rbtree_node)
};

/**
 * Declare a rbtree.
 */
#define RBTREE_DECL(tree) BTREE_DECL(rbtree_node, tree)

/**
 * Init a to-be-inserted btree node.
 *
 * The root node has parent NULL.
 */
#define RBTREE_NODE_INIT(node) do {             \
        (node).color = RB_RED;                  \
        BTREE_NODE_INIT(node);                  \
    } while (/*CONSTCOND*/ 0)

BTREE_PROTOTYPE(rbtree, rbtree_node)

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
 * Rotate in one direction at child of @root, then in the other direction @dir
 * at @root.
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

/* DELETE */
/* If the node to be deleted is red, we're done. */
/* If node has a child (=> single and red), recolor it to black. */
/* If node to be deleted is black, we need to correct. */
/* - if child red: recolor to black */
/* - if child black: check sibling: */
/*   - sibling is red */
/*   - sibling is black and children are: */
/*     - two black */
/*     - right red */
/*     - left red, right black */

#endif /* RBTREE_H */


#ifdef RBTREE_CREATE

#define BTREE_NAME RBTREE_NAME
#define BTREE_TYPE RBTREE_TYPE
#define BTREE_NODE_TYPE RBTREE_NODE_TYPE
#define BTREE_NODE_MEMBER RBTREE_NODE_MEMBER
#define BTREE_KEY_TYPE RBTREE_KEY_TYPE
#define BTREE_KEY_MEMBER RBTREE_KEY_MEMBER

#define BTREE_NODE_NAME rbtree

#define BTREE_CREATE
#include "btree_impl.h"

#undef RBTREE_NODE_MEMBER
#undef RBTREE_KEY_TYPE
#undef RBTREE_KEY_MEMBER
#undef RBTREE_NODE_TYPE
#undef RBTREE_TYPE
#undef RBTREE_NAME
#undef RBTREE_CREATE

#endif // defined RBTREE_CREATE
