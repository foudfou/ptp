#ifndef BSTREE_H
#define BSTREE_H

/**
 * A basic, non-balanced, binary search tree.
 *
 * Inspired by the Linux kernel and Julienne Walker.
 *
 * Consumer MUST provide a key comparison function:
 *
 *   int name##_compare(BSTREE_KEY_TYPE keyA, BSTREE_KEY_TYPE keyB);
 */
#include <stdbool.h>
#include <stddef.h>
#include "base.h"
#include "cont.h"

#define BSTREE_MEMBERS(node_t)     \
    struct node_t * parent;        \
    struct node_t * link[2];
#define LEFT  0
#define RIGHT 1

struct bstree_node {
    BSTREE_MEMBERS(bstree_node)
};

#define RIGHT_IF(cond) (cond) ? RIGHT : LEFT

#define BSTREE_DECL(tree) BSTREE_GENERIC_DECL(bstree_node, tree)
#define BSTREE_GENERIC_DECL(type, tree) struct type *tree = NULL

#define BSTREE_NODE_INIT(node) do { (node).parent = NULL;       \
        (node).link[LEFT] = NULL; (node).link[RIGHT] = NULL;    \
    } while (/*CONSTCOND*/ 0)

#define BSTREE_GENERATE(name, type, field, key)     \
    BSTREE_GENERATE_DELETE(name, type,)             \
    BSTREE_GENERATE_INSERT(name, type, field, key,) \
    BSTREE_GENERATE_SEARCH(name, type, field, key)

#define BSTREE_NODE(type) struct type##_node

#define BSTREE_GENERATE_BASE(type)              \
/**
 * Test wether a tree is empty.
 */                                                                 \
static inline bool type##_is_empty(BSTREE_NODE(type) *tree)         \
{                                                                   \
    return !tree;                                                   \
}                                                                   \
                                                                    \
/**
 * Link a node to a parent node.
 *
 * @target can be either a link (address of next node) or
 * the tree (address of first node).
 */                                                                     \
static inline void type##_link_node(BSTREE_NODE(type) *node,            \
                                    BSTREE_NODE(type) *parent,          \
                                    BSTREE_NODE(type) **target)         \
{                                                                       \
    if (node)                                                           \
        node->parent = parent;                                          \
    *target = node;                                                     \
}                                                                       \
                                                                        \
static inline BSTREE_NODE(type)*                                        \
__##type##_end(BSTREE_NODE(type) *tree, int dir)                        \
{                                                                       \
    if (!tree) return NULL;                                             \
                                                                        \
    /* Make sure we're on the root. */                                  \
    while (tree->parent)                                                \
        tree = tree->parent;                                            \
                                                                        \
    while (tree->link[dir])                                             \
        tree = tree->link[dir];                                         \
    return tree;                                                        \
}                                                                       \
                                                                        \
/**
 * Find the first, lowest node.
 */                                                                     \
static inline BSTREE_NODE(type) *type##_first(BSTREE_NODE(type) *tree)  \
{                                                                       \
    return __##type##_end(tree, LEFT);                                  \
}                                                                       \
                                                                        \
/**
 * Find the last, highest node.
 */                                                                     \
static inline BSTREE_NODE(type) *type##_last(BSTREE_NODE(type) *tree)   \
{                                                                       \
    return __##type##_end(tree, RIGHT);                                 \
}                                                                       \
                                                                        \
/**
 * Find the next/previous inorder node.
 */                                                                     \
static inline BSTREE_NODE(type)*                                        \
__##type##_iterate(const BSTREE_NODE(type) *node, int dir)              \
{                                                                       \
    if (!node) return NULL;                                             \
                                                                        \
    BSTREE_NODE(type) *it = (BSTREE_NODE(type) *)node;                  \
    /* last opposite child after dir child */                           \
    if ((it = node->link[dir])) {                                       \
        while (it->link[!dir])                                          \
            it = it->link[!dir];                                        \
    }                                                                   \
    /* first dir parent after last opposite parent, or null */          \
    else {                                                              \
        while ((it = node->parent) && it->link[dir] == node)            \
            node = it;                                                  \
    }                                                                   \
    return it;                                                          \
}                                                                       \
                                                                        \
/**
 * Find the next inorder node.
 */                                                                     \
static inline BSTREE_NODE(type)* type##_next(const BSTREE_NODE(type) *node) \
{                                                                       \
    return __##type##_iterate(node, RIGHT);                             \
}                                                                       \
                                                                        \
/**
 * Find the previous inorder node.
 */                                                                     \
static inline BSTREE_NODE(type)* type##_prev(const BSTREE_NODE(type) *node) \
{                                                                       \
    return __##type##_iterate(node, LEFT);                              \
}                                                                       \
/**
 * Get the parent link to a given node, which may be the tree itself.
 */                                                                     \
static inline BSTREE_NODE(type)**                                       \
type##_parent_link(BSTREE_NODE(type) **tree, BSTREE_NODE(type) *node)   \
{                                                                       \
    return node->parent ?                                               \
        &(node->parent->link[RIGHT_IF(node->parent->link[RIGHT] == node)]) : \
        tree;                                                           \
}
BSTREE_GENERATE_BASE(bstree)

#define BSTREE_GENERATE_DELETE(name, type, opt)   \
/**
 * Delete a node.
 *
 * @node may be modified to point to the actually deleted node !
 */                                                                     \
static inline bool name##opt##_delete(BSTREE_NODE(type) **tree,         \
                                      BSTREE_NODE(type) *node)          \
{                                                                       \
    if (!*tree || !node)                                                \
        return false;                                                   \
                                                                        \
    BSTREE_NODE(type) **parent_link = type##_parent_link(tree, node);   \
                                                                        \
    if (node->link[LEFT] && node->link[RIGHT]) { /* delete by swapping */ \
        /* we could also take the predecessor */                        \
        BSTREE_NODE(type) *succ = node->link[RIGHT];                    \
        while (succ->link[LEFT]) {                                      \
            succ = succ->link[LEFT];                                    \
        }                                                               \
        BSTREE_NODE(type) deleted = *succ;                              \
                                                                        \
        if (succ != node->link[RIGHT]) {                                \
            /* link succ's only right child to succ's parent */         \
            BSTREE_NODE(type) *succ_parent = succ->parent;              \
            type##_link_node(succ->link[RIGHT], succ_parent,            \
                             &(succ_parent->link[LEFT]));               \
            type##_link_node(node->link[RIGHT], succ, &(succ->link[RIGHT])); \
        }                                                               \
        else {                                                          \
            /* Set parent to the future parent. This trick enables us to find
               the sibling afterwards. */                               \
            deleted.parent = succ;                                      \
        }                                                               \
                                                                        \
        /* Transplant succ to node's place, which is equivalent to: swapping
           the surrounding structs' nodes + updating succ's new parent link. */ \
        type##_link_node(succ, node->parent, parent_link);              \
        type##_link_node(node->link[LEFT], succ, &(succ->link[LEFT]));  \
                                                                        \
        *node = deleted;                                                \
    }                                                                   \
    /*
     *        p          p
     *         \          \
     *          x    ->    ~
     *         / \
     *        ~   ~
     */                                                             \
    else if (!node->link[LEFT] && !node->link[RIGHT]) {             \
        *parent_link = NULL;                                        \
    }                                                               \
    /*
     *        p           p             p           p
     *         \           \             \           \
     *          x     ->    c    OR       x    ->     c
     *           \                       /
     *            c                     c
     */                                                                 \
    else {                                                              \
        int child_dir = RIGHT_IF(node->link[LEFT] == NULL);             \
        type##_link_node(node->link[child_dir], node->parent, parent_link); \
    }                                                                   \
                                                                        \
    return true;                                                        \
}

#define BSTREE_GENERATE_INSERT(name, type, field, key, opt) \
/**
 * Insert a node into a tree.
 */                                                                 \
static inline bool                                                  \
name##opt##_insert(BSTREE_NODE(type) **tree, struct name *data)     \
{                                                                   \
    /* We'll iterate on the *link* fields, which enables us to look up the
       next node while keeping a hold on the current node. Hence the double
       pointer. */                                                  \
    BSTREE_NODE(type) **it = tree, *parent = NULL;                  \
                                                                    \
    while (*it) {                                                   \
        struct name *this = cont(*it, struct name, field);          \
        int result = name##_compare(data->key, this->key);          \
                                                                    \
        if (result == 0)                                            \
            return false;       /* already there */                 \
        else {                                                      \
            parent = *it;                                           \
            it = &((*it)->link[RIGHT_IF(result > 0)]);              \
        }                                                           \
    }                                                               \
                                                                    \
    type##_link_node(&data->field, parent, it);                     \
                                                                    \
    return true;                                                    \
}

#define BSTREE_GENERATE_SEARCH(name, type, field, key)  \
/**
 * Gets an entry by its key.
 *
 * Returns NULL when not found.
 */                                                          \
static inline struct name*                                   \
name##_search(BSTREE_NODE(type) *tree, BSTREE_KEY_TYPE val)  \
{                                                            \
    BSTREE_NODE(type) *it = tree;                            \
                                                             \
    while (it) {                                             \
        struct name *this = cont(it, struct name, field);    \
        int result = name##_compare(val, this->key);         \
                                                             \
        if (result == 0)                                     \
            return this;                                     \
        else                                                 \
            it = it->link[RIGHT_IF(result > 0)];             \
    }                                                        \
    return NULL;                                             \
}

#endif /* BSTREE_H */
