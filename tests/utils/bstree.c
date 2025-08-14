/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "utils/cont.h"
#include "utils/bstree.h"

static const unsigned int KEY_MAX_LENGTH = 1024;

/* Define a bstree type. */
struct foo {
    struct bstree_node node;
    const char * key;
};
/* We MUST provide our own key comparison function. See bstree.h. */
static inline int foo_compare(const char * keyA, const char * keyB)
{
    return strncmp(keyA, keyB, KEY_MAX_LENGTH);
}
#define BSTREE_KEY_TYPE const char *
BSTREE_GENERATE(foo, bstree, node, key)

/* For debugging, recover the definition from the SCM. */
void bstree_display(struct bstree_node *root);

int main ()
{
    /* Btree declaration */
    BSTREE_DECL(tree);
    assert(!tree);
    assert(bstree_is_empty(tree));
    assert(!foo_search(tree, "hello"));

    /* artificial tree */
    struct foo t1 = { {NULL, {NULL,}}, "eee"};
    tree = &(t1.node);
    assert(foo_search(tree, "eee"));
    struct foo t2 = { {NULL, {NULL,}}, "aaa"};
    struct foo t3 = { {NULL, {NULL,}}, "mmm"};
    t1.node.link[LEFT] = &(t2.node);
    t1.node.link[RIGHT] = &(t3.node);
    assert(foo_search(tree, "aaa"));
    assert(foo_search(tree, "mmm"));

    /* start over */
    tree = NULL;
    assert(bstree_is_empty(tree));
    BSTREE_NODE_INIT(t1.node);
    BSTREE_NODE_INIT(t2.node);
    BSTREE_NODE_INIT(t3.node);

    /* Btree insertion */
    assert(foo_insert(&tree, &t1));
    assert(tree == &(t1.node));
    assert(!(*tree).parent);
    assert(foo_search(tree, "eee"));
    assert(!foo_search(tree, "mmm"));
    assert(!foo_insert(&tree, &t1));
    assert(foo_insert(&tree, &t2));
    assert(foo_insert(&tree, &t3));
    assert(t1.node.link[LEFT] == &(t2.node));
    assert(t1.node.link[RIGHT] == &(t3.node));
    assert(foo_search(tree, "mmm"));

    /* Btree deletion */
    struct foo t4 = { {NULL, {NULL,}}, "rrr"};
    assert(foo_insert(&tree, &t4));
    /*
     *   e
     *  / \
     * a   m
     *      \
     *       r
     */
    foo_delete(&tree, &t3.node);
    assert(!foo_search(tree, "mmm"));
    assert(t1.node.link[LEFT] == &(t2.node));

    assert(t1.node.link[RIGHT] == &(t4.node));
    assert(foo_delete(&tree, &t4.node));
    assert(!foo_search(tree, "rrr"));
    assert(t1.node.link[LEFT] == &(t2.node));
    assert(!t1.node.link[RIGHT]);

    BSTREE_DECL(pair);
    struct foo p1 = { {NULL, {NULL,}}, "1"};
    struct foo p2 = { {NULL, {NULL,}}, "2"};
    assert(foo_insert(&pair, &p1));
    assert(foo_insert(&pair, &p2));
    assert(foo_delete(&pair, &p2.node));

    BSTREE_DECL(numbers);
    struct foo one    = { {NULL, {NULL,}}, "1"};
    struct foo two    = { {NULL, {NULL,}}, "2"};
    struct foo four   = { {NULL, {NULL,}}, "4"};
    struct foo five   = { {NULL, {NULL,}}, "5"};
    struct foo seven  = { {NULL, {NULL,}}, "7"};
    struct foo eight  = { {NULL, {NULL,}}, "8"};
    struct foo nine   = { {NULL, {NULL,}}, "9"};
    struct foo eleven = { {NULL, {NULL,}}, "B"};
    assert(foo_insert(&numbers, &two));
    assert(foo_insert(&numbers, &one));
    assert(foo_insert(&numbers, &five));
    assert(foo_insert(&numbers, &four));
    assert(foo_insert(&numbers, &nine));
    assert(foo_insert(&numbers, &seven));
    assert(foo_insert(&numbers, &eight));
    assert(foo_insert(&numbers, &eleven));

    /*
     *    2               2
     *   / \             / \
     *  1   5*          1   7
     *     / \             / \
     *    4   9           4   9
     *       / \             / \
     *     +7   B           8   B
     *       \
     *        8
     */
    assert(foo_delete(&numbers, &five.node));
    assert(!foo_search(numbers, "5"));
    assert(numbers == &two.node);
    assert(nine.node.link[LEFT] == &(eight.node));
    assert(eight.node.parent == &(nine.node));
    assert(seven.node.link[LEFT] == &(four.node));
    assert(four.node.parent == &(seven.node));
    assert(seven.node.link[RIGHT] == &(nine.node));
    assert(nine.node.parent == &(seven.node));
    assert(seven.node.parent == &(two.node));
    assert(seven.node.parent->link[RIGHT] == &(seven.node));
    /* deleted node is the actually deleted node */
    assert(five.node.parent == &nine.node);
    assert(!five.node.link[LEFT]);
    assert(five.node.link[RIGHT] == &eight.node);

    /*
     *    2*
     *   / \
     *  1   7
     *     / \
     *    4+  9
     *       / \
     *      8   B
     */
    assert(foo_delete(&numbers, &two.node));
    assert(numbers == &four.node);
    assert(four.node.link[LEFT] == &(one.node));
    assert(four.node.link[RIGHT] == &(seven.node));
    assert(!seven.node.link[LEFT]);
    /* the actual deleted node is preserved */
    assert(two.node.parent == &seven.node);
    assert(!two.node.link[LEFT]);
    assert(!two.node.link[RIGHT]);

    /*
     *    4           4
     *   / \         / \
     *  1   7       1   7
     *       \           \
     *        9*          B
     *       / \         /
     *      8   B+      8
     */
    /* successor is to-be-deleted node's right child */
    assert(foo_delete(&numbers, &nine.node));
    assert(eleven.node.parent == &seven.node);
    assert(eleven.node.link[LEFT] == &eight.node);
    assert(!eleven.node.link[RIGHT]);
    assert(nine.node.parent == &eleven.node);
    assert(!nine.node.link[LEFT]);
    assert(!nine.node.link[RIGHT]);

    /*
     *    2               2
     *   / \             / \
     *  1   6*          1   9
     *     / \             /
     *    4   9           4
     *   / \             / \
     *  3   5           3   5
     */

    /* Bstree navigation/traversal */
    int DIGITS_LEN = 10;
    char *DIGITS_CHAR[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
    struct foo digits_ary[DIGITS_LEN];
    for (int i=0; i<DIGITS_LEN; ++i)
        digits_ary[i] = (struct foo) { {NULL, {NULL,}}, DIGITS_CHAR[i] };
    int *digits_ins = NULL; // compound literals instead of malloc and memcpy
    BSTREE_DECL(digits);

    int digits_ins_start = 1;
    int digits_ins_len = 9;
    digits_ins = (int[]) {2,1,4,3,8,5,9,6,7};
    for (int i=0; i<digits_ins_len; ++i)
        assert(foo_insert(&digits, &digits_ary[digits_ins[i]]));
    /*
     *    2
     *   / \
     *  1   4
     *     / \
     *    3   8
     *       / \
     *      5   9
     *       \
     *        6
     *         \
     *          7
     */

    /* First/Last */
    assert(bstree_first(digits) == &(digits_ary[1].node));
    assert(!bstree_first(NULL));
    assert(bstree_first(&(digits_ary[8].node)) == &(digits_ary[1].node));
    assert(bstree_first(&(digits_ary[3].node)) == &(digits_ary[1].node));
    assert(bstree_first(&(digits_ary[1].node)) == &(digits_ary[1].node));
    assert(bstree_last(digits) == &(digits_ary[9].node));
    assert(!bstree_last(NULL));
    assert(bstree_last(&(digits_ary[8].node)) == &(digits_ary[9].node));
    assert(bstree_last(&(digits_ary[3].node)) == &(digits_ary[9].node));
    assert(bstree_last(&(digits_ary[9].node)) == &(digits_ary[9].node));

    /* Next */
    for (int i=digits_ins_start; i<(digits_ins_len-1); ++i)
        assert(bstree_next(&(digits_ary[i].node)) == &(digits_ary[i+1].node));

    /* reset */
    digits = NULL;
    for (int i=0; i<DIGITS_LEN; ++i)
        digits_ary[i] = (struct foo) { {NULL, {NULL,}}, DIGITS_CHAR[i] };

    digits_ins_start = 0;
    digits_ins_len = 10;
    digits_ins = (int[]) {2,1,5,0,4,9,3,7,6,8};
    for (int i=0; i<digits_ins_len; ++i)
        assert(foo_insert(&digits, &digits_ary[digits_ins[i]]));
    /*
     *        2
     *       / \
     *      1   5
     *     /   / \
     *    0   4   9
     *       /   /
     *      3   7
     *         / \
     *        6   8
     */

    /* Next */
    for (int i=digits_ins_start; i<(digits_ins_len-1); ++i)
        assert(bstree_next(&(digits_ary[i].node)) == &(digits_ary[i+1].node));
    assert(!bstree_next(&(digits_ary[9].node)));

    /* Previous */
    for (int i=(digits_ins_len - 1); i>digits_ins_start; --i)
        assert(bstree_prev(&(digits_ary[i].node)) == &(digits_ary[i-1].node));
    assert(!bstree_prev(&(digits_ary[0].node)));

    /* Traversal */
    struct bstree_node *it = bstree_first(digits);
    const char *expected = "0123456789"; int i = 0;
    while (it) {
        assert((cont(it, struct foo, node)->key)[0] == expected[i]);
        it = bstree_next(it); i++;
    }


    return 0;
}
