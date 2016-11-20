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
RBTREE_GENERATE(foo, rbtree_node, node, key, _bs)

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
    /*
     *     5
     *    / \
     *   3  10
     *      / \
     *     7   15
     */
    rbtree_display(&n5.node);

    assert(rbtree_rotate(&n5.node, LEFT) == &n10.node);
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

    assert(rbtree_rotate(&n10.node, RIGHT) == &n5.node);
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
    assert(foo_is_empty(tree));

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

    assert(rbtree_rotate(tree, RIGHT) == &n5.node);
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

    digits = NULL;
    digits_ins = (uint32_t[]) {11,7,14,2,1,5,8,4};
    digits_ins_len = 8;
    for (int i=0; i<digits_ins_len; ++i) {
        digits_ary[i] = (struct foo) FOO_INIT(RB_RED, NULL, NULL, NULL,
                                              digits_ins[i])
        assert(foo_insert(&digits, &digits_ary[i]));
    }
    assert(rbtree_validate(digits) == 3);


    return 0;
}
