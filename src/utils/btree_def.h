#ifndef BTREE_DEF_H
#define BTREE_DEF_H
/**
 * Base functions for all binary trees. Insired by
 * https://github.com/freebsd/freebsd/blob/master/sys/sys/tree.h Yeah baby!
 */

#include <stdbool.h>
#include <stddef.h>
#include "base.h"
#include "cont.h"

#define BTREE_MEMBERS(type)              \
    struct type * parent;                \
    struct type * link[2];

#define LEFT  0
#define RIGHT 1

#define RIGHT_IF(cond) (cond) ? RIGHT : LEFT

#define BTREE_DECL(type, tree) struct type *tree = NULL

#define BTREE_NODE_INIT(node) do { (node).parent = NULL;        \
        (node).link[LEFT] = NULL; (node).link[RIGHT] = NULL;    \
    } while (/*CONSTCOND*/ 0)

#define BTREE_PROTOTYPE(name, type)                                     \
    /**                                                                 \
     * Test wether a tree is empty.                                     \
     */                                                                 \
    static inline bool name##_is_empty(struct type *tree)               \
    {                                                                   \
        return !tree;                                                   \
    }                                                                   \
                                                                        \
    /**                                                                 \
     * Link a node to a parent node.                                    \
     *                                                                  \
     * @target can be either a link (address of next node) or           \
     * the tree (address of first node).                                \
     */                                                                 \
    static inline void name##_link_node(struct type *node,              \
                                        struct type *parent,            \
                                        struct type **target)           \
    {                                                                   \
        node->parent = parent;                                          \
        *target = node;                                                 \
    }                                                                   \
                                                                        \
    /**                                                                 \
     * Delete a node.                                                   \
     */                                                                 \
    static inline bool name##_delete(struct type **tree,                \
                                     struct type *node)                 \
    {                                                                   \
        if (!*tree || !node)                                            \
            return false;                                               \
                                                                        \
        struct type **parent_link = tree;                               \
        if (node->parent) {                                             \
            int dir = RIGHT_IF(node->parent->link[RIGHT] == node);      \
            parent_link = &(node->parent->link[dir]);                   \
        }                                                               \
                                                                        \
        if (node->link[LEFT] && node->link[RIGHT]) { /* delete by swapping */ \
            struct type *succ = node->link[RIGHT];                      \
            while (succ->link[LEFT]) {                                  \
                succ = succ->link[LEFT];                                \
            }                                                           \
                                                                        \
            /* link succ's only right child to succ's parent */         \
            if (succ->link[RIGHT]) {                                    \
                struct type *succ_parent = succ->parent;                \
                int succ_dir = RIGHT_IF(succ_parent->link[RIGHT] == succ); \
                name##_link_node(succ->link[RIGHT], succ_parent,        \
                                 &(succ_parent->link[succ_dir]));       \
            }                                                           \
                                                                        \
            name##_link_node(succ, node->parent, parent_link);          \
            name##_link_node(node->link[LEFT], succ, &(succ->link[LEFT])); \
            name##_link_node(node->link[RIGHT], succ, &(succ->link[RIGHT])); \
        }                                                               \
        /*                                                              \
         *        p          p                                          \
         *         \          \                                         \
         *          x    ->    ~                                        \
         *         /                                                    \
         *        ~   ~                                                 \
         */                                                             \
        else if (!node->link[LEFT] && !node->link[RIGHT]) {             \
            *parent_link = NULL;                                        \
        }                                                               \
        /*                                                              \
         *        p           p             p           p               \
         *         \           \             \           \              \
         *          x     ->    c    OR       x    ->     c             \
         *           \                       /                          \
         *            c                     c                           \
         */                                                             \
        else {                                                          \
            int child_dir = RIGHT_IF(node->link[LEFT] == NULL);         \
            name##_link_node(node->link[child_dir], node->parent, parent_link); \
        }                                                               \
                                                                        \
        return true;                                                    \
    }                                                                   \
                                                                        \
    static inline struct type *__##name##_end(struct type *tree, int dir) \
    {                                                                   \
        if (!tree) return NULL;                                         \
                                                                        \
        /* Make sure we're on the root. */                              \
        while (tree->parent)                                            \
            tree = tree->parent;                                        \
                                                                        \
        while (tree->link[dir])                                         \
            tree = tree->link[dir];                                     \
        return tree;                                                    \
    }                                                                   \
                                                                        \
    /**                                                                 \
     * Find the first, lowest node.                                     \
     */                                                                 \
    static inline struct type *name##_first(struct type *tree)          \
    {                                                                   \
        return __##name##_end(tree, LEFT);                              \
    }                                                                   \
                                                                        \
    /**                                                                 \
     * Find the last, highest node.                                     \
     */                                                                 \
    static inline struct type *name##_last(struct type *tree)           \
    {                                                                   \
        return __##name##_end(tree, RIGHT);                             \
    }                                                                   \
                                                                        \
    /**                                                                 \
     * Find the next/previous inorder node.                             \
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
    /**                                                                 \
     * Find the next inorder node.                                      \
     */                                                                 \
    static inline struct type *name##_next(const struct type *node)     \
    {                                                                   \
        return __##name##_iterate(node, RIGHT);                         \
    }                                                                   \
                                                                        \
    /**                                                                 \
     * Find the previous inorder node.                                  \
     */                                                                 \
    static inline struct type *name##_prev(const struct type *node)     \
    {                                                                   \
        return __##name##_iterate(node, LEFT);                          \
    }

#endif /* BTREE_DEF_H */
