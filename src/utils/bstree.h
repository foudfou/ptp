#ifndef BSTREE_H
#define BSTREE_H
/**
 * A basic, non-balanced, binary search tree.
 *
 * Inspired by the Linux kernel and Julienne Walker.
 *
 * This header can be imported multiple times with BSTREE_CREATE to create
 * different bstree types. Just make sure to use a different BSTREE_NAME.
 *
 * Consumer MUST provide a key comparison function:
 *
 * int BSTREE_FUNCTION(compare)(BSTREE_KEY_TYPE keyA, BSTREE_KEY_TYPE keyB);
 */
#include "btree_def.h"

struct bstree_node {
    BTREE_MEMBERS(bstree_node)
};

/**
 * Declare a bstree.
 */
#define BSTREE_DECL(tree) BTREE_DECL(bstree_node, tree)

/**
 * Init a to-be-inserted btree node.
 *
 * The root node has parent NULL.
 */
#define BSTREE_NODE_INIT(node) BTREE_NODE_INIT(node)

BTREE_PROTOTYPE(bstree, bstree_node)

#endif /* BSTREE_H */


#ifdef BSTREE_CREATE

#define BTREE_NAME BSTREE_NAME
#define BTREE_TYPE BSTREE_TYPE
#define BTREE_NODE_TYPE BSTREE_NODE_TYPE
#define BTREE_NODE_MEMBER BSTREE_NODE_MEMBER
#define BTREE_KEY_TYPE BSTREE_KEY_TYPE
#define BTREE_KEY_MEMBER BSTREE_KEY_MEMBER

#define BTREE_NODE_NAME bstree

#define BTREE_CREATE
#include "btree_impl.h"

#undef BSTREE_NODE_MEMBER
#undef BSTREE_KEY_TYPE
#undef BSTREE_KEY_MEMBER
#undef BSTREE_NODE_TYPE
#undef BSTREE_TYPE
#undef BSTREE_NAME
#undef BSTREE_CREATE

#endif // defined BSTREE_CREATE
