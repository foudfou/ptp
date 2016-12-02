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

#define BSTREE_MEMBERS(type)                    \
    struct type * parent;                       \
    struct type * link[2];

struct bstree_node {
    BSTREE_MEMBERS(bstree_node)
};

/* LEFT and RIGHT are arbitrary opposite values, in the boolean termm, in order
   to use the (!direction) property. */
#define LEFT  0
#define RIGHT 1

#define RIGHT_IF(cond) (cond) ? RIGHT : LEFT

#define BSTREE_DECL(tree) BSTREE_GENERIC_DECL(bstree_node, tree)
#define BSTREE_GENERIC_DECL(type, tree) struct type *tree = NULL

#define BSTREE_NODE_INIT(node) do { (node).parent = NULL;       \
        (node).link[LEFT] = NULL; (node).link[RIGHT] = NULL;    \
    } while (/*CONSTCOND*/ 0)

#define BSTREE_GENERATE(name, type, field, key)     \
    BSTREE_GENERATE_BASE(name, type)                \
    BSTREE_GENERATE_DELETE(name, type,)             \
    BSTREE_GENERATE_INSERT(name, type, field, key,) \
    BSTREE_GENERATE_SEARCH(name, type, field, key)

#define BSTREE_GENERATE_BASE(name, type)        \
/**
 * Test wether a tree is empty.
 */                                                                 \
static inline bool name##_is_empty(struct type *tree)               \
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
static inline void name##_link_node(struct type *node,                  \
                                    struct type *parent,                \
                                    struct type **target)               \
{                                                                       \
    if (node)                                                           \
        node->parent = parent;                                          \
    *target = node;                                                     \
}                                                                       \
                                                                        \
static inline struct type *__##name##_end(struct type *tree, int dir)   \
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
 */                                                                 \
static inline struct type *name##_first(struct type *tree)          \
{                                                                   \
    return __##name##_end(tree, LEFT);                              \
}                                                                   \
                                                                    \
/**
 * Find the last, highest node.
 */                                                                 \
static inline struct type *name##_last(struct type *tree)           \
{                                                                   \
    return __##name##_end(tree, RIGHT);                             \
}                                                                   \
                                                                    \
/**
 * Find the next/previous inorder node.
 */                                                                 \
static inline struct type*                                          \
__##name##_iterate(const struct type *node, int dir)                \
{                                                                   \
    if (!node) return NULL;                                         \
                                                                    \
    struct type *it = (struct type *)node;                          \
    /* last opposite child after dir child */                       \
    if ((it = node->link[dir])) {                                   \
        while (it->link[!dir])                                      \
            it = it->link[!dir];                                    \
    }                                                               \
    /* first dir parent after last opposite parent, or null */      \
    else {                                                          \
        while ((it = node->parent) && it->link[dir] == node)        \
            node = it;                                              \
    }                                                               \
    return it;                                                      \
}                                                                   \
                                                                    \
/**
 * Find the next inorder node.
 */                                                                 \
static inline struct type *name##_next(const struct type *node)     \
{                                                                   \
    return __##name##_iterate(node, RIGHT);                         \
}                                                                   \
                                                                    \
/**
 * Find the previous inorder node.
 */                                                                 \
static inline struct type *name##_prev(const struct type *node)     \
{                                                                   \
    return __##name##_iterate(node, LEFT);                          \
}

#define BSTREE_GENERATE_DELETE(name, type, opt)   \
/**
 * Delete a node.
 *
 * @node may be modified to point to the actually deleted node !
 */                                                                     \
static inline bool name##opt##_delete(struct type **tree,               \
                                      struct type *node)                \
{                                                                       \
    if (!*tree || !node)                                                \
        return false;                                                   \
                                                                        \
    struct type **parent_link = tree;                                   \
    if (node->parent) {                                                 \
        int dir = RIGHT_IF(node->parent->link[RIGHT] == node);          \
        parent_link = &(node->parent->link[dir]);                       \
    }                                                                   \
                                                                        \
    if (node->link[LEFT] && node->link[RIGHT]) { /* delete by swapping */ \
        /* we could also take the predecessor */                        \
        struct type *succ = node->link[RIGHT];                          \
        while (succ->link[LEFT]) {                                      \
            succ = succ->link[LEFT];                                    \
        }                                                               \
        struct type deleted = *succ;                                    \
                                                                        \
        if (succ != node->link[RIGHT]) {                                \
            /* link succ's only right child to succ's parent */         \
            struct type *succ_parent = succ->parent;                    \
            name##_link_node(succ->link[RIGHT], succ_parent,            \
                             &(succ_parent->link[LEFT]));               \
            name##_link_node(node->link[RIGHT], succ, &(succ->link[RIGHT])); \
        }                                                               \
                                                                        \
        /* Transplant succ to node's place, which is equivalent to: swapping
           the surrounding structs' nodes + updating succ's new parent link. */ \
        name##_link_node(succ, node->parent, parent_link);              \
        name##_link_node(node->link[LEFT], succ, &(succ->link[LEFT]));  \
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
        name##_link_node(node->link[child_dir], node->parent, parent_link); \
    }                                                                   \
                                                                        \
    return true;                                                        \
}

#define BSTREE_GENERATE_INSERT(name, type, field, key, opt) \
/**
 * Insert a node into a tree.
 */                                                                 \
static inline bool                                                  \
name##opt##_insert(struct type **tree, struct name *data)           \
{                                                                   \
    /* We'll iterate on the *link* fields, which enables us to look up the
       next node while keeping a hold on the current node. Hence the double
       pointer. */                                                  \
    struct type **it = tree, *parent = NULL;                        \
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
    name##_link_node(&data->field, parent, it);                     \
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
name##_search(struct type *tree, BSTREE_KEY_TYPE val)        \
{                                                            \
    struct type *it = tree;                                  \
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
