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

#include <stdbool.h>
#include <stddef.h>
#include "base.h"
#include "cont.h"

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

#define RIGHT_IF(cond) (cond) ? RIGHT : LEFT

/**
 * Link a node to a parent node.
 *
 * @target can be either a link (address of next node) or the tree (address of
 * first node).
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
    if (!*tree || !node)
        return false;

    struct bstree_node **parent_link = tree;
    if (node->parent) {
        int dir = RIGHT_IF(node->parent->link[RIGHT] == node);
        parent_link = &(node->parent->link[dir]);
    }

    if (node->link[LEFT] && node->link[RIGHT]) { // delete by swapping
        struct bstree_node *succ = node->link[RIGHT];
        while (succ->link[LEFT]) {
            succ = succ->link[LEFT];
        }

        // link succ's only right child to succ's parent
        if (succ->link[RIGHT]) {
            struct bstree_node *succ_parent = succ->parent;
            int succ_dir = RIGHT_IF(succ_parent->link[RIGHT] == succ);
            bstree_link_node(succ->link[RIGHT], succ_parent,
                             &(succ_parent->link[succ_dir]));
        }

        bstree_link_node(succ, node->parent, parent_link);
        bstree_link_node(node->link[LEFT], succ, &(succ->link[LEFT]));
        bstree_link_node(node->link[RIGHT], succ, &(succ->link[RIGHT]));
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
           \                       /
            c                     c
     */
    else {
        int child_dir = RIGHT_IF(node->link[LEFT] == NULL);
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

    struct bstree_node *it = (struct bstree_node *)node;
    // last opposite child after dir child
    if ((it = node->link[dir])) {
        while (it->link[!dir])
            it = it->link[!dir];
    }
    // first dir parent after last opposite parent, or null
    else {
        while ((it = node->parent) && it->link[dir] == node)
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

#ifdef BSTREE_CREATE

#ifndef BSTREE_NAME
    #error must give this map type a name by defining BSTREE_NAME
#endif
#ifndef BSTREE_TYPE
    #error must define BSTREE_TYPE
#endif
#ifndef BSTREE_NODE_MEMBER
    #error must define BSTREE_NODE_MEMBER
#endif
#if defined(BSTREE_KEY_TYPE)
    assert_compilation(!(sizeof(BSTREE_KEY_TYPE) % 4));
#else
    #error must define BSTREE_KEY_TYPE
#endif
#ifndef BSTREE_KEY_MEMBER
    #error must define BSTREE_KEY_MEMBER
#endif

#define BSTREE_FUNCTION(name)                                       \
    BSTREE_GLUE(BSTREE_GLUE(BSTREE_GLUE(bstree_, BSTREE_NAME), _), name)
#define BSTREE_GLUE(x, y) BSTREE_GLUE2(x, y)
#define BSTREE_GLUE2(x, y) x ## y

/**
 * Insert a node into a tree.
 */
static inline bool
BSTREE_FUNCTION(insert)(struct bstree_node **tree, BSTREE_TYPE *data)
{
    /* We'll iterate on the *link* fields, which enables us to look for the
       next node while keeping a hold on the current node. Hence the double
       pointer. */
    struct bstree_node **it = tree, *parent = NULL;

    while (*it) {
        BSTREE_TYPE *this = cont(*it, BSTREE_TYPE, BSTREE_NODE_MEMBER);
        int result = BSTREE_FUNCTION(compare)(data->BSTREE_KEY_MEMBER,
                                              this->BSTREE_KEY_MEMBER);

        if (result == 0)
            return false;       // already there
        else {
            parent = *it;
            it = &((*it)->link[RIGHT_IF(result > 0)]);
        }
    }

    bstree_link_node(&data->BSTREE_NODE_MEMBER, parent, it);

    return true;
}

/**
 * Gets an entry by its key.
 *
 * Returns NULL when not found.
 */
static inline BSTREE_TYPE*
BSTREE_FUNCTION(search)(struct bstree_node *tree, BSTREE_KEY_TYPE key)
{
    struct bstree_node *it = tree;

    while (it) {
        BSTREE_TYPE *data = cont(it, BSTREE_TYPE, BSTREE_NODE_MEMBER);
        int result = BSTREE_FUNCTION(compare)(key, data->BSTREE_KEY_MEMBER);

        if (result == 0)
            return data;
        else
            it = it->link[RIGHT_IF(result > 0)];
    }
    return NULL;
}


#undef BSTREE_NODE_MEMBER
#undef BSTREE_FUNCTION
#undef BSTREE_KEY_TYPE
#undef BSTREE_KEY_MEMBER
#undef BSTREE_TYPE
#undef BSTREE_NAME
#undef BSTREE_CREATE

#endif // defined BSTREE_CREATE
