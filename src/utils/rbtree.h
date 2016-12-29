#ifndef RBTREE_H
#define RBTREE_H

/**
 * A red-black binary tree.
 *
 * Better than hash if order matters.
 * Inspired by the Linux kernel and Julienne Walker.
 *
 * Consumer MUST provide a key comparison function:
 *
 *   int name##_compare(RBTREE_KEY_TYPE keyA, RBTREE_KEY_TYPE keyB);
 */
#include "bstree.h"

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
    BSTREE_MEMBERS(rbtree_node)
};

/**
 * Declare a rbtree.
 */
#define RBTREE_DECL(tree) BSTREE_GENERIC_DECL(rbtree_node, tree)

/**
 * Init a to-be-inserted btree node.
 *
 * The root node has parent NULL.
 */
#define RBTREE_NODE_INIT(node) do {             \
        (node).color = RB_RED;                  \
        BSTREE_NODE_INIT(node);                 \
    } while (/*CONSTCOND*/ 0)


#define BSTREE_KEY_TYPE RBTREE_KEY_TYPE
#define RBTREE_GENERATE(name, type, field, key, opt)    \
    BSTREE_GENERATE_DELETE(name, type, opt)             \
    BSTREE_GENERATE_INSERT(name, type, field, key, opt) \
    BSTREE_GENERATE_SEARCH(name, type, field, key)      \
    RBTREE_GENERATE_INSERT(name, field, opt)            \
    RBTREE_GENERATE_DELETE(name, opt)

BSTREE_GENERATE_BASE(rbtree)

/**
 * Rotate at @root in direction @dir.
 *
 *     x             y
 *    / \           / \
 *   a   y   <->   x   c
 *      / \       / \
 *     b   c     a   b
 */
void rbtree_rotate(struct rbtree_node *root, int dir,
                   struct rbtree_node **parent_link)
{
    struct rbtree_node *new = root->link[!dir];

    rbtree_link_node(new->link[dir], root, &(root->link[!dir]));
    rbtree_link_node(new, root->parent, parent_link);
    rbtree_link_node(root, new, &(new->link[dir]));
}

/**
 * Double rotation
 *
 * Rotate in opposite direction at child of @root, then in the @dir direction
 * at @root.
 *
 *       z*               z             y
 *      / \              / \          /   \
 *     x   d            y   d        x     z
 *    / \       ->     / \     ->   / \   / \
 *   a   y            x   c        a   b c   d
 *      / \          / \
 *     b   c        a   b
 */
void rbtree_rotate_double(struct rbtree_node *root, int dir,
                          struct rbtree_node **parent_link)
{
    rbtree_rotate(root->link[!dir], !dir, &(root->link[!dir]));
    rbtree_rotate(root, dir, parent_link);
}

#define RBTREE_GENERATE_INSERT(name, field, opt)                        \
static inline bool                                                      \
name##_insert(struct rbtree_node **tree, struct name *data)             \
{                                                                       \
    if (!name##opt##_insert(tree, data))                                \
        return false;                                                   \
                                                                        \
    struct rbtree_node *node = &data->field;                            \
    while (node->parent) {                                              \
        node->color = RB_RED;                                           \
                                                                        \
        /* Parent black, nothing to do. */                              \
        if (node->parent->color == RB_BLACK)                            \
            return true;                                                \
                                                                        \
        /* Red parent => has a grand-parent. */                         \
        struct rbtree_node *parent = node->parent;                      \
        int par_dir = RIGHT_IF(parent == parent->parent->link[RIGHT]);  \
        struct rbtree_node *uncle = parent->parent->link[!par_dir];     \
                                                                        \
        /*
         * Red uncle => reverse relatives' color.
         *
         *    B        r
         *   / \      / \
         *  r   r -> B   B
         *   \        \
         *    r        r
         */                                                             \
        if (uncle && uncle->color == RB_RED) {                          \
            parent->color = RB_BLACK;                                   \
            uncle->color = RB_BLACK;                                    \
            parent->parent->color = RB_RED;                             \
            node = parent->parent;                                      \
        }                                                               \
        /*
         * Black or no uncle => rotate and recolor.
         */                                                             \
        else {                                                          \
            int dir = RIGHT_IF(node == parent->link[RIGHT]);            \
            struct rbtree_node **top = rbtree_parent_link(tree, parent->parent); \
            /*
             *       B         (r)B
             *      / \        /   \
             *     r   B  ->  r*   (B)r
             *    / \              / \
             *   r*  B            B   B
             */                                                         \
            if (par_dir == dir)                                         \
                rbtree_rotate(parent->parent, !par_dir, top);           \
            /*
             *      B         (r*)B
             *     / \        /   \
             *    r   B  ->  r    (B)r
             *     \                \
             *      r*               B
             */                                                         \
            else                                                        \
                rbtree_rotate_double(parent->parent, !par_dir, top);    \
            (*top)->color = RB_BLACK;                                   \
            (*top)->link[!par_dir]->color = RB_RED;                     \
        }                                                               \
    }                                                                   \
                                                                        \
    node->color = RB_BLACK;                                             \
    return true;                                                        \
}

/* http://horstmann.com/unblog/2011-05-12/blog.html
   http://www.geeksforgeeks.org/red-black-tree-set-3-delete-2/
   https://www.cs.purdue.edu/homes/ayg/CS251/slides/chap13c.pdf for an
   overview, but the most reliable is definitely "Introduction to Algorithms",
   MIT Press. */
#define RBTREE_GENERATE_DELETE(name, opt)                               \
static inline bool                                                      \
name##_delete(struct rbtree_node **tree, struct rbtree_node *node)      \
{                                                                       \
    struct rbtree_node **parent_link_orig = rbtree_parent_link(tree, node); \
    struct rbtree_node *parent_orig = node->parent;                     \
    int color_orig = node->color;                                       \
                                                                        \
    /* First perfom a bstree delete. Note that, because of delete-by-swap, we
       end up deleting a node with at most 1 child. */                  \
    if (!name##opt##_delete(tree, node))                                \
        return false;                                                   \
                                                                        \
    /* Test if delete-by-swap occured. We could also test if node had 2
       children bferohand. Now node still points to the same struct but its
       bstree values are now those of the deepest actually deleted node.
       *parent_link_orig however points to the replacing node. */       \
    if (*parent_link_orig && node->parent != parent_orig)               \
        (*parent_link_orig)->color = color_orig;                        \
                                                                        \
    if (node->color == RB_RED)                                          \
        return true;                                                    \
                                                                        \
    struct rbtree_node *child = node->link[LEFT] ? node->link[LEFT] :   \
        node->link[RIGHT];                                              \
    if (child && child->color == RB_RED) {                              \
        child->color = RB_BLACK;                                        \
        return true;                                                    \
    }                                                                   \
                                                                        \
    /*
     * At this stage, both node and child (if any) are black. Now, having
     * removed a black node, we virtually color the child as double-black. If
     * node has no child, then the trick is to consider its NULL children a new
     * double-black NULL child.
     *
     *       30b         30b            30b            30b
     *      /  \   →    /   \    OR    /  \   →       /   \
     *    20b* 40b    10bb  40b      20b* 40b   (NULL)bb  40b
     *    /
     *  10b
     */                                                             \
                                                                    \
    /* Starting from the new child, we'll consider his sibling. */  \
    struct rbtree_node *parent = node->parent;                      \
    while (parent && !(child && child->color == RB_RED)) {          \
        /* We know there is a sibling otherwise the tree would be
           unbalanced. */                                               \
        int child_dir = RIGHT_IF(parent->link[RIGHT] == child);         \
        struct rbtree_node *sibling = parent->link[!child_dir];         \
                                                                        \
        struct rbtree_node **parent_link = rbtree_parent_link(tree, parent); \
        if (sibling->color == RB_RED) {                                 \
            sibling->color = RB_BLACK;                                  \
            parent->color = RB_RED;                                     \
            rbtree_rotate(parent, child_dir, parent_link);              \
            sibling = parent->link[!child_dir];                         \
            continue;                                                   \
        }                                                               \
                                                                        \
        struct rbtree_node * nephew_aligned = sibling->link[!child_dir]; \
        struct rbtree_node * nephew_unaligned = sibling->link[child_dir]; \
        int neph_a_color = nephew_aligned ? nephew_aligned->color : RB_BLACK; \
        int neph_u_color = nephew_unaligned ? nephew_unaligned->color : RB_BLACK; \
                                                                        \
        if (neph_a_color == RB_BLACK && neph_u_color == RB_BLACK) {     \
            sibling->color = RB_RED;                                    \
            child = parent;                                             \
            parent = parent->parent;                                    \
            continue;                                                   \
        }                                                               \
                                                                        \
        if (neph_a_color == RB_RED) {                                   \
            sibling->color = parent->color;                             \
            parent->color = RB_BLACK;                                   \
            nephew_aligned->color = RB_BLACK;                           \
            rbtree_rotate(parent, child_dir, parent_link);              \
        }                                                               \
        else {                                                          \
            nephew_unaligned->color = parent->color;                    \
            parent->color = RB_BLACK;                                   \
            rbtree_rotate_double(parent, child_dir, parent_link);       \
        }                                                               \
        break;                                                          \
    }                                                                   \
                                                                        \
    if (child)                                                          \
        child->color = RB_BLACK;                                        \
                                                                        \
    return true;                                                        \
}

#endif /* RBTREE_H */
