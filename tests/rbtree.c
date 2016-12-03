#include <assert.h>
#include <stdlib.h>
#include "utils/cont.h"
#include "utils/rbtree.h"

/* Define a rbtree type. */
struct foo {
    struct rbtree_node node;
    uint32_t key;
};
/* We MUST provide our own key comparison function. See rbtree.h. */
static inline int foo_compare(uint32_t keyA, uint32_t keyB)
{
    return keyA - keyB;
}
#define RBTREE_KEY_TYPE uint32_t
RBTREE_GENERATE(foo, rbtree, node, key, _bs)


#include <stdio.h>
void rbtree_display(struct rbtree_node *root)
{
    if (!root) {
        printf(".");
        return;
    }

    struct rbtree_node *ln = root->link[LEFT];
    struct rbtree_node *rn = root->link[RIGHT];

    struct foo *this = cont(root, struct foo, node);
    printf("{%d(%d)=", this->key, root->color);
    rbtree_display(ln);
    printf(",");
    rbtree_display(rn);
    printf("} ");
}

/* http://horstmann.com/unblog/2011-05-12/blog.html
   http://www.geeksforgeeks.org/red-black-tree-set-3-delete-2/ */
static inline bool foo_delete(struct rbtree_node **tree,
                              struct rbtree_node *node)
{
    struct rbtree_node **parent_link_orig = rbtree_parent_link(tree, node);
    int color_orig = node->color;

    /* first perfom a bstree delete. Note that, because of delete-by-swap, we
       end up deleting a node with at most 1 child. */
    if (!foo_bs_delete(tree, node))
        return false;
    rbtree_display(*tree); fflush(stdout);

// FIXME: is the (*parent_link_orig) test sufficient ?
    if (*parent_link_orig && node != *parent_link_orig) /* delete-by-swap */
        (*parent_link_orig)->color = color_orig;

    /* If the deleted node is red, we're done. */
    if (node->color == RB_RED)
        return true;

    /* If deleted node is black, we need to correct. */
    /* - if child red: recolor to black */
    struct rbtree_node *child = node->link[LEFT] ? node->link[LEFT] :
        node->link[RIGHT];
    if (child && child->color == RB_RED) {
        child->color = RB_BLACK;
        return true;
    }

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
     */

    /* Starting from the new child, we'll consider his sibling. */
    struct rbtree_node *parent = node->parent;
    do {
        /* We know there is a sibling otherwise the tree would be
           unbalanced. */
        int child_dir = RIGHT_IF(parent->link[RIGHT] == child);
// FIXME: whatif node->parent == NULL ?
        struct rbtree_node *sibling = parent->link[!child_dir];

        /* - if child black: check sibling: */
        /*   - sibling is red: */
        struct rbtree_node **parent_link = rbtree_parent_link(tree, parent);
        if (sibling->color == RB_RED) {
            /* rotate to move sibling up */
            rbtree_rotate(parent, child_dir, parent_link);
            /* recolor the old sibling and parent */
            sibling->color = RB_BLACK;
            parent->color = RB_RED;
            continue;
        }

        struct rbtree_node * nephew_aligned = sibling->link[!child_dir];
        struct rbtree_node * nephew_unaligned = sibling->link[child_dir];
        int neph_a_color = nephew_aligned ? nephew_aligned->color : RB_BLACK;
        int neph_u_color = nephew_unaligned ? nephew_unaligned->color : RB_BLACK;

        /*   - sibling is black and children are: */
        /*     - two black */
        if (neph_a_color == RB_BLACK && neph_u_color == RB_BLACK) {
            sibling->color = RB_RED;
            child = parent;
            parent = parent->parent;
            continue;
        }

        /*     - right red */
        /*     - left red, right black */
        if (neph_a_color == RB_RED) {
            rbtree_rotate(sibling, child_dir, &(parent->link[!child_dir]));
            nephew_aligned->color = RB_BLACK;
        }
        else {
            rbtree_rotate_double(parent, child_dir, parent_link);
            nephew_unaligned->color = RB_BLACK;
        }
        break;
    } while (child->color == RB_BLACK && parent);

    rbtree_display(*tree); fflush(stdout);
    child->color = RB_BLACK;

    return true;
}

#define IS_RED(node) (node != NULL && (node)->color == RB_RED)

/* http://www.eternallyconfuzzled.com/tuts/datastructures/jsw_tut_rbtree.aspx */
int rbtree_validate(struct rbtree_node *root)
{
    if (!root)
        return 1;

    struct rbtree_node *ln = root->link[LEFT];
    struct rbtree_node *rn = root->link[RIGHT];

    /* Red violation / Consecutive red links */
    assert(!(IS_RED(root) && (IS_RED(ln) || IS_RED(rn))));

    int lh = rbtree_validate(ln);
    int rh = rbtree_validate(rn);

    /* Binary tree violation / Invalid binary search tree */
    uint32_t root_key = cont(root, struct foo, node)->key;
    assert(!((ln && cont(ln, struct foo, node)->key >= root_key) ||
             (rn && cont(rn, struct foo, node)->key <= root_key)));

    /* Black violation / Black height mismatch */
    assert(!(lh != 0 && rh != 0 && lh != rh));

    /* Only count black links */
    if (lh != 0 && rh != 0)
        return IS_RED(root) ? lh : lh + 1; // just count the left-hand side
    else
        return 0;
}

#define LEN(ary)  (sizeof(ary) / sizeof(ary[0]))

#define FOO_INIT(color, parent, left, right, key)    \
    {{(color), (parent), {(left), (right)}}, (key)};

int main ()
{
    /* Rotate */
    struct foo n5 = FOO_INIT(RB_RED, NULL, NULL, NULL, 5);
    struct foo n3 = FOO_INIT(RB_RED, &n5.node, NULL, NULL, 3)
    n5.node.link[LEFT] = &n3.node;
    struct foo n10 = FOO_INIT(RB_RED, &n5.node, NULL, NULL, 10)
    n5.node.link[RIGHT] = &n10.node;
    struct foo n7 = FOO_INIT(RB_RED, &n10.node, NULL, NULL, 7)
    n10.node.link[LEFT] = &n7.node;
    struct foo n15 = FOO_INIT(RB_RED, &n10.node, NULL, NULL, 15)
    n10.node.link[RIGHT] = &n15.node;
    struct rbtree_node *tr0 = &n5.node;
    /*
     *     5
     *    / \
     *   3  10
     *      / \
     *     7   15
     */

    rbtree_rotate(&n5.node, LEFT, &tr0);
    assert(tr0 == &n10.node);
    /*
     *    10
     *    / \
     *   5  15
     *  / \
     * 3   7
     */
    assert(!n10.node.parent);
    assert(n10.node.link[LEFT] == &n5.node);
    assert(n5.node.parent == &n10.node);
    assert(n10.node.link[RIGHT] == &n15.node);
    assert(n15.node.parent == &n10.node);
    assert(n5.node.link[LEFT] == &n3.node);
    assert(n3.node.parent == &n5.node);
    assert(n5.node.link[RIGHT] == &n7.node);
    assert(n7.node.parent == &n5.node);

    rbtree_rotate(&n10.node, RIGHT, &tr0);
    assert(tr0 == &n5.node);
    /*
     *     5
     *    / \
     *   3  10
     *      / \
     *     7   15
     */
    assert(!n5.node.parent);
    assert(n5.node.link[RIGHT] == &n10.node);
    assert(n10.node.parent == &n5.node);
    assert(n5.node.link[LEFT] == &n3.node);
    assert(n3.node.parent == &n5.node);
    assert(n10.node.link[LEFT] == &n7.node);
    assert(n10.node.link[RIGHT] == &n15.node);
    assert(n7.node.parent == &n10.node);
    assert(n15.node.parent == &n10.node);

    /* Btree declaration */
    RBTREE_DECL(tree);
    assert(!tree);
    assert(rbtree_is_empty(tree));

    /* Tree insertion */
    RBTREE_NODE_INIT(n5.node); RBTREE_NODE_INIT(n10.node);
    RBTREE_NODE_INIT(n3.node); RBTREE_NODE_INIT(n7.node);
    RBTREE_NODE_INIT(n15.node);
    /*
     *    10
     *    / \
     *   5  15
     *  / \
     * 3   7
     */
    assert(foo_bs_insert(&tree, &n10));
    assert(tree);
    assert(tree == &n10.node);
    assert(!foo_bs_insert(&tree, &n10));
    assert(foo_bs_insert(&tree, &n5));
    assert(n10.node.link[LEFT] == &n5.node);
    assert(n5.node.parent == &n10.node);
    assert(foo_bs_insert(&tree, &n15));
    assert(n10.node.link[RIGHT] == &n15.node);
    assert(n15.node.parent == &n10.node);
    assert(foo_bs_insert(&tree, &n3));
    assert(n5.node.link[LEFT] == &n3.node);
    assert(n3.node.parent == &n5.node);

    assert(rbtree_first(tree) == &n3.node);
    assert(rbtree_last(tree) == &n15.node);
    assert(rbtree_next(&n5.node) == &n10.node);
    assert(rbtree_prev(&n10.node) == &n5.node);
    assert(!foo_search(tree, 7));
    assert(foo_bs_insert(&tree, &n7));
    assert(foo_search(tree, 7) == &n7);
    assert(foo_bs_delete(&tree, &n7.node));
    assert(!foo_search(tree, 7));

    rbtree_rotate(tree, RIGHT, &tree);
    assert(tree == &n5.node);
    /*
     *     5
     *    / \
     *   3  10
     *      / \
     *     7   15
     */

    /* Reset */
    tree = NULL;
    RBTREE_NODE_INIT(n5.node); RBTREE_NODE_INIT(n10.node);
    RBTREE_NODE_INIT(n3.node); RBTREE_NODE_INIT(n7.node);
    RBTREE_NODE_INIT(n15.node);

    /* RB-Tree insertion
     *
     *      10
     *     / \
     *    5   15
     *   / \
     *  3   7
     */
    assert(foo_insert(&tree, &n10));
    assert(tree == &n10.node);
    assert(n10.node.color == RB_BLACK);
    assert(!foo_insert(&tree, &n10));

    assert(foo_insert(&tree, &n5));
    assert(n5.node.color == RB_RED);

    assert(foo_insert(&tree, &n15));
    assert(n15.node.color == RB_RED);

    assert(foo_insert(&tree, &n7)); // red uncle
    assert(rbtree_validate(tree) == 3);
    assert(tree == &n10.node);
    assert(n10.node.color == RB_BLACK);
    assert(n5.node.color == RB_BLACK);
    assert(n15.node.color == RB_BLACK);
    assert(n7.node.color == RB_RED);

    assert(foo_insert(&tree, &n3));

    RBTREE_DECL(digits);
    uint32_t *digits_ins = NULL; // compound literals instead of malloc and memcpy
    digits_ins = (uint32_t[]) {2,1,4,3,8,5,9,6,7};
    /*
     *        __4__
     *       /     \
     *      2       8
     *     / \     / \
     *    1   3   6   9
     *           / \
     *          5  7
     */
    int digits_ins_len = 9;
    struct foo digits_ary[digits_ins_len];
    for (int i=0; i<digits_ins_len; i++) {
        digits_ary[i] = (struct foo) FOO_INIT(RB_RED, NULL, NULL, NULL,
                                              digits_ins[i])
        assert(foo_insert(&digits, &digits_ary[i]));
    }
    assert(rbtree_validate(digits) == 3);

    /*
     *     __7b__                      __7b__
     *    /      \                    /      \
     *   2r      11r*     bs_del     2r      14b(r)
     *  /  \     / \     ------->   /  \     /
     * 1b   5b  8b  14b+           1b   5b  8b
     *     /                           /
     *    4r                          4r
     */
    digits = NULL;
    digits_ins = (uint32_t[]) {11,7,14,2,1,5,8,4};
    digits_ins_len = 8;
    for (int i=0; i<digits_ins_len; ++i) {
        digits_ary[i] = (struct foo) FOO_INIT(RB_RED, NULL, NULL, NULL,
                                              digits_ins[i])
        assert(foo_insert(&digits, &digits_ary[i]));
    }
    assert(rbtree_validate(digits) == 3);

    assert(foo_delete(&digits, &digits_ary[0].node)); // 11
    rbtree_display(digits); fflush(stdout);
    assert(rbtree_validate(digits) == 3);

    assert(foo_delete(&digits, &digits_ary[4].node)); // 1
    rbtree_display(digits); fflush(stdout);
    assert(rbtree_validate(digits) == 3);

    /* assert(foo_delete(&digits, &digits_ary[1].node)); // 7 */
    /* rbtree_display(digits); fflush(stdout); */
    /* assert(rbtree_validate(digits) == 3); */

    return 0;
}
