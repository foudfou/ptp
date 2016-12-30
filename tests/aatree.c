#include <assert.h>
#include <stdlib.h>
#include "utils/cont.h"
#include "utils/aatree.h"

/* Define a aatree type. */
struct foo {
    struct aatree_node node;
    uint32_t key;
};
/* We MUST provide our own key comparison function. See rbtree.h. */
static inline int foo_compare(uint32_t keyA, uint32_t keyB)
{
    return keyA - keyB;
}
#define AATREE_KEY_TYPE uint32_t
AATREE_GENERATE(foo, aatree, node, key)

#define FOO_INIT(level, parent, left, right, key)  \
    {{(level), (parent), {(left), (right)}}, (key)}

/*
               4,3
        /               \
     2,2                 10,3
    /   \            /          \
 1,1     3,1      6,2            12,2
                 /   \          /    \
              5,1     8,2   11,1      13,1
                     /   \
                  7,1     9,1
 */
static inline bool
foo_insert(struct aatree_node **tree, struct foo *data)
{
    if (!foo_bs_insert(tree, data))
        return false;

    struct aatree_node *node = &data->node;
    while (node) {
        struct aatree_node **top = aatree_parent_link(tree, node);
        aatree_skew(node, top);
        aatree_split(node, top);
        node = node->parent;
    }

    return true;
}

/* /\* FIXME: *\/ */
/* int aatree_validate(struct aatree_node *root) */
/* { */
/*     if (!root) */
/*         return 1; */

/*     struct aatree_node *ln = root->link[LEFT]; */
/*     struct aatree_node *rn = root->link[RIGHT]; */

/*     /\* Red violation / Consecutive red links *\/ */
/*     assert(!(IS_RED(root) && (IS_RED(ln) || IS_RED(rn)))); */

/*     int lh = aatree_validate(ln); */
/*     int rh = aatree_validate(rn); */

/*     /\* Binary tree violation / Invalid binary search tree *\/ */
/*     uint32_t root_key = cont(root, struct foo, node)->key; */
/*     assert(!((ln && cont(ln, struct foo, node)->key >= root_key) || */
/*              (rn && cont(rn, struct foo, node)->key <= root_key))); */

/*     /\* Black violation / Black height mismatch *\/ */
/*     assert(!(lh != 0 && rh != 0 && lh != rh)); */

/*     /\* Only count black links *\/ */
/*     if (lh != 0 && rh != 0) */
/*         return IS_RED(root) ? lh : lh + 1; // just count the left-hand side */
/*     else */
/*         return 0; */
/* } */

#include <stdio.h>
void aatree_display(struct aatree_node *root)
{
    if (!root) {
        printf(".");
        return;
    }

    struct aatree_node *ln = root->link[LEFT];
    struct aatree_node *rn = root->link[RIGHT];

    struct foo *this = cont(root, struct foo, node);
    printf("{%d(%d)=", this->key, root->level);
    aatree_display(ln);
    printf(",");
    aatree_display(rn);
    printf("} ");
}

int main ()
{
    int nlen = 16;
    struct foo n[nlen];
    for (int i = 0; i < nlen; i++)
        n[i] = (struct foo) FOO_INIT(0, NULL, NULL, NULL, i+1);

    /* Btree declaration */
    AATREE_DECL(aa1);
    assert(!aa1);
    assert(aatree_is_empty(aa1));

    /* Tree insertion */
    for (int i = 0; i < nlen; i++) {
        AATREE_NODE_INIT(n[i].node);
        assert(foo_insert(&aa1, &n[i]));
    }
    /* {8(4)={4(3)={2(2)={1(1)=.,.} ,{3(1)=.,.} } ,{6(2)={5(1)=.,.} ,{7(1)=.,.} } } ,{12(3)={10(2)={9(1)=.,.} ,{11(1)=.,.} } ,{14(2)={13(1)=.,.} ,{15(1)=.,{16(1)=.,.} } } } } */


    return 0;
}
