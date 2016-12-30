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
    BSTREE_GENERATE_SEARCH(name, type, field, key)
    /* AATREE_GENERATE_INSERT(name, field, opt)                    \ */
    /* AATREE_GENERATE_DELETE(name, opt) */

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

#endif /* AATREE_H */
