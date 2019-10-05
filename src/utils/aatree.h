/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#ifndef AATREE_H
#define AATREE_H

/**
 * An Anderson tree is a balanced bstree, simpler and as efficient as a rbtree.
 *
 * Consumer MUST provide a key comparison function:
 *
 *   int name##_compare(RBTREE_KEY_TYPE keyA, RBTREE_KEY_TYPE keyB);
 *
 * See
 * http://www.eternallyconfuzzled.com/tuts/datastructures/jsw_tut_andersson.aspx
 * https://en.wikipedia.org/wiki/AA_tree
 */
#include "bstree.h"

/**
 * AA-tree rules:
 * 1) Every path contains the same number of pseudo-nodes.
 * 2) A left child may not have the same level as its parent, hence skew().
 * 3) Two right children with the same level as the parent are not allowed,
 *    hence split().
 */
struct aatree_node {
    unsigned int level;
    BSTREE_MEMBERS(aatree_node)
};

/**
 * Declare an aatree.
 */
#define AATREE_DECL(tree) BSTREE_GENERIC_DECL(aatree_node, tree)

/**
 * Init a to-be-inserted aatree node.
 *
 * The root node has parent NULL.
 */
#define AATREE_NODE_INIT(node) do {             \
        (node).level = 1;                       \
        BSTREE_NODE_INIT(node);                 \
    } while (/*CONSTCOND*/ 0)

#define BSTREE_KEY_TYPE AATREE_KEY_TYPE
#define AATREE_GENERATE(name, type, field, key)             \
    AATREE_GENERATE_INTERNAL(name, type, field, key, _bs)

#define AATREE_GENERATE_INTERNAL(name, type, field, key, opt)   \
    BSTREE_GENERATE_DELETE(name, type, opt)                     \
    BSTREE_GENERATE_INSERT(name, type, field, key, opt)         \
    BSTREE_GENERATE_SEARCH(name, type, field, key)              \
    AATREE_GENERATE_INSERT(name, field, opt)                    \
    AATREE_GENERATE_DELETE(name, opt)

// cppcheck-suppress ctunullpointer
BSTREE_GENERATE_BASE(aatree)


/**
 * Skew is similar to right-rotate.
 *
 *         d,2               b,2
 *        /   \             /   \
 *     b,2     e,1  -->  a,1     d,2
 *    /   \                     /   \
 * a,1     c,1               c,1     e,1
 */
static inline void aatree_skew(struct aatree_node *root,
                               struct aatree_node **parent_link)
{
    if (root->link[LEFT] && root->link[LEFT]->level == root->level) {
        struct aatree_node *new = root->link[LEFT];
        aatree_link_node(new->link[RIGHT], root, &(root->link[LEFT]));
        aatree_link_node(new, root->parent, parent_link);
        aatree_link_node(root, new, &(new->link[RIGHT]));
    }
}

/**
 * Split is similar to left-rotate.
 *
 *      b,2                     d,3
 *     /   \                   /   \
 *  a,1     d,2     -->     b,2     e,2
 *         /   \           /   \
 *      c,1     e,2     a,1     c,1
 */
static inline void aatree_split(struct aatree_node *root,
                                struct aatree_node **parent_link)
{
    if (root->link[RIGHT] && root->link[RIGHT]->link[RIGHT] &&
        root->link[RIGHT]->link[RIGHT]->level == root->level) {
        struct aatree_node *new = root->link[RIGHT];
        aatree_link_node(new->link[LEFT], root, &(root->link[RIGHT]));
        aatree_link_node(new, root->parent, parent_link);
        aatree_link_node(root, new, &(new->link[LEFT]));
        new->level += 1;
    }
}

#define AATREE_GENERATE_INSERT(name, field, opt)                    \
static inline bool                                                  \
name##_insert(struct aatree_node **tree, struct name *data)         \
{                                                                   \
    if (!foo_bs_insert(tree, data))                                 \
        return false;                                               \
                                                                    \
    struct aatree_node *node = &data->field;                        \
    while (node) {                                                  \
        struct aatree_node **top = aatree_parent_link(tree, node);  \
        aatree_skew(node, top);                                     \
        aatree_split(node, top);                                    \
        node = node->parent;                                        \
    }                                                               \
                                                                    \
    return true;                                                    \
}

#define AATREE_GENERATE_DELETE(name, opt)                               \
static inline bool                                                      \
name##_delete(struct aatree_node **tree, struct aatree_node *node)      \
{                                                                       \
    struct aatree_node **parent_link_orig = aatree_parent_link(tree, node); \
    struct aatree_node *parent_orig = node->parent;                     \
    int level_orig = node->level;                                       \
                                                                        \
    if (!name##opt##_delete(tree, node))                                \
        return false;                                                   \
                                                                        \
    /* Test if delete-by-swap occured. */                               \
    if (*parent_link_orig && node->parent != parent_orig)               \
        (*parent_link_orig)->level = level_orig;                        \
                                                                        \
    while (node) {                                                      \
        unsigned int levell = node->link[LEFT] ? node->link[LEFT]->level : 0; \
        unsigned int levelr = node->link[RIGHT] ? node->link[RIGHT]->level : 0; \
        if ((levell < node->level - 1) || (levelr < node->level - 1)) { \
            node->level -= 1;                                           \
            if (levelr > node->level)                                   \
                node->link[RIGHT]->level = node->level;                 \
                                                                        \
            struct aatree_node **top = aatree_parent_link(tree, node);  \
            aatree_skew(node, top);                                     \
            if ((*top)->link[RIGHT])                                    \
                aatree_skew((*top)->link[RIGHT], &((*top)->link[RIGHT])); \
            if ((*top)->link[RIGHT]->link[RIGHT])                       \
                aatree_skew((*top)->link[RIGHT]->link[RIGHT],           \
                            &((*top)->link[RIGHT]->link[RIGHT]));       \
            aatree_split((*top), top);                                  \
            aatree_split((*top)->link[RIGHT], &((*top)->link[RIGHT]));  \
        }                                                               \
                                                                        \
        node = node->parent;                                            \
    }                                                                   \
                                                                        \
    return true;                                                        \
}

#endif /* AATREE_H */
