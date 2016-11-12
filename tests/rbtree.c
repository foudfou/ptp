#include <assert.h>
#include <stdlib.h>
#include "../src/utils/cont.h"
#include "../src/utils/rbtree_defs.h"
#include "../src/utils/rbtree.h"

struct mytype {
    struct rbtree_node node;
    uint32_t key;
};

int my_compare(uint32_t keyA, uint32_t keyB)
{
    return keyA - keyB;
}

#include <stdio.h>
bool my_bstree_insert(struct rbtree_node **tree, struct mytype *data)
{
    if (rbtree_is_empty(*tree)) {
        *tree = &data->node;
        return true;
    }

    struct rbtree_node *child = *tree, *parent = NULL;
    int dir = 0;

    while (child) {
        struct mytype *data_child = cont(child, struct mytype, node);
        int cmp = my_compare(data->key, data_child->key);

        if (cmp == 0)
            return false;       // already there
        else {
            parent = child;
            dir = RB_RIGHT_IF(cmp > 0);
            child = (child)->link[dir];
        }
    }

    rbtree_link_node(parent, &data->node, dir);

    return true;
}

bool my_rb_insert(struct rbtree_node **tree, struct mytype *data)
{
    if (!my_bstree_insert(tree, data))
        return false;

    struct rbtree_node *node = &data->node;
    while (node->parent) {
        node->color = RB_RED;

        /* Parent black, nothing to do. */
        if (node->parent->color == RB_BLACK) {
            /* printf("black parent\n"); */
            return true;
        }

        /* Red parent => has a grand-parent. */
        struct rbtree_node *parent = node->parent;
        int par_dir = RB_RIGHT_IF(parent == parent->parent->link[RB_RIGHT]);
        struct rbtree_node *uncle = parent->parent->link[!par_dir];

        /*
         * Red uncle => reverse relatives' color.
         *
         *    B        r
         *   / \      / \
         *  r   r -> B   B
         *   \        \
         *    r        r
         */
        if (uncle && uncle->color == RB_RED) {
            /* printf("red uncle\n"); */
            parent->color = RB_BLACK;
            uncle->color = RB_BLACK;
            parent->parent->color = RB_RED;
            node = parent->parent;
        }
        /*
         * Black or no uncle => rotate and recolor.
         */
        else {
            /* printf("--black uncle-->%d\n", !par_dir); */
            int dir = RB_RIGHT_IF(node == parent->link[RB_RIGHT]);
            struct rbtree_node *top;
            /*
             *       B         (r)B
             *      / \        /   \
             *     r   B  ->  r*   (B)r
             *    / \              / \
             *   r*  B            B   B
             */
            if (par_dir == dir)
                top = rbtree_rotate(parent->parent, !par_dir);
            /*
             *      B         (r*)B
             *     / \        /   \
             *    r   B  ->  r    (B)r
             *     \                \
             *      r*               B
             */
            else
                top = rbtree_rotate_double(parent->parent, !par_dir);
            top->color = RB_BLACK;
            top->link[!par_dir]->color = RB_RED;

            if (top->parent) {
                int orig_dir = RB_RIGHT_IF(
                    top->parent->link[RB_RIGHT] == parent->parent);
                top->parent->link[orig_dir] = top;
            }
            else
                *tree = top;
        }
    }

    node->color = RB_BLACK;
    return true;
}


#define IS_RED(node) (node != NULL && (node)->color == RB_RED)

/* http://www.eternallyconfuzzled.com/tuts/datastructures/jsw_tut_rbtree.aspx */
int rbtree_validate(struct rbtree_node *root)
{
    if (!root)
        return 1;

    struct rbtree_node *ln = root->link[RB_LEFT];
    struct rbtree_node *rn = root->link[RB_RIGHT];

    /* Red violation / Consecutive red links */
    assert(!(IS_RED(root) && (IS_RED(ln) || IS_RED(rn))));

    struct mytype *this = cont(root, struct mytype, node); /* DEBUG */
    printf("{%d(%d)", this->key, root->color);             /* DEBUG */
    int lh = rbtree_validate(ln);
    printf(", ");             /* DEBUG */
    int rh = rbtree_validate(rn);
    printf("}\n");             /* DEBUG */

    /* Binary tree violation / Invalid binary search tree */
    uint32_t root_key = cont(root, struct mytype, node)->key;
    assert(!((ln && cont(ln, struct mytype, node)->key >= root_key) ||
             (rn && cont(rn, struct mytype, node)->key <= root_key)));

    /* Black violation / Black height mismatch */
    assert(!(lh != 0 && rh != 0 && lh != rh));

    /* Only count black links */
    if (lh != 0 && rh != 0)
        return IS_RED(root) ? lh : lh + 1; // just count the left-hand side
    else
        return 0;
}

void rbtree_display(struct rbtree_node *root)
{
    if (!root) {
        printf(".");
        return;
    }

    struct rbtree_node *ln = root->link[RB_LEFT];
    struct rbtree_node *rn = root->link[RB_RIGHT];

    struct mytype *this = cont(root, struct mytype, node);
    printf("{%d(%d)=", this->key, root->color);
    rbtree_display(ln);
    printf(",");
    rbtree_display(rn);
    printf("} ");
}

#define LEN(ary)  (sizeof(ary) / sizeof(ary[0]))

int main ()
{
    /* Rotate */
    struct mytype n5 = {{0, NULL, {NULL}}, 5};
    struct mytype n3 = {{0, &n5.node, {NULL}}, 3};
    n5.node.link[RB_LEFT] = &n3.node;
    struct mytype n10 = {{0, &n5.node, {NULL}}, 10};
    n5.node.link[RB_RIGHT] = &n10.node;
    struct mytype n7 = {{0, &n10.node, {NULL}}, 7};
    n10.node.link[RB_LEFT] = &n7.node;
    struct mytype n15 = {{0, &n10.node, {NULL}}, 15};
    n10.node.link[RB_RIGHT] = &n15.node;
    /*
     *     5
     *    / \
     *   3  10
     *      / \
     *     7   15
     */
    rbtree_display(&n5.node);

    assert(rbtree_rotate(&n5.node, RB_LEFT) == &n10.node);
    /*
     *    10
     *    / \
     *   5  15
     *  / \
     * 3   7
     */
    assert(!n10.node.parent);
    assert(n10.node.link[RB_LEFT] == &n5.node);
    assert(n5.node.parent == &n10.node);
    assert(n10.node.link[RB_RIGHT] == &n15.node);
    assert(n15.node.parent == &n10.node);
    assert(n5.node.link[RB_LEFT] == &n3.node);
    assert(n3.node.parent == &n5.node);
    assert(n5.node.link[RB_RIGHT] == &n7.node);
    assert(n7.node.parent == &n5.node);

    assert(rbtree_rotate(&n10.node, RB_RIGHT) == &n5.node);
    /*
     *     5
     *    / \
     *   3  10
     *      / \
     *     7   15
     */
    assert(!n5.node.parent);
    assert(n5.node.link[RB_RIGHT] == &n10.node);
    assert(n10.node.parent == &n5.node);
    assert(n5.node.link[RB_LEFT] == &n3.node);
    assert(n3.node.parent == &n5.node);
    assert(n10.node.link[RB_LEFT] == &n7.node);
    assert(n10.node.link[RB_RIGHT] == &n15.node);
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
    assert(my_bstree_insert(&tree, &n10));
    assert(tree);
    assert(tree == &n10.node);
    assert(!my_bstree_insert(&tree, &n10));
    assert(my_bstree_insert(&tree, &n5));
    assert(n10.node.link[RB_LEFT] == &n5.node);
    assert(n5.node.parent == &n10.node);
    assert(my_bstree_insert(&tree, &n15));
    assert(n10.node.link[RB_RIGHT] == &n15.node);
    assert(n15.node.parent == &n10.node);
    assert(my_bstree_insert(&tree, &n3));
    assert(n5.node.link[RB_LEFT] == &n3.node);
    assert(n3.node.parent == &n5.node);

    assert(rbtree_rotate(tree, RB_RIGHT) == &n5.node);
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
    assert(my_rb_insert(&tree, &n10));
    assert(tree == &n10.node);
    assert(n10.node.color == RB_BLACK);
    assert(!my_rb_insert(&tree, &n10));

    assert(my_rb_insert(&tree, &n5));
    assert(n5.node.color == RB_RED);

    assert(my_rb_insert(&tree, &n15));
    assert(n15.node.color == RB_RED);

    assert(my_rb_insert(&tree, &n7)); // red uncle
    assert(rbtree_validate(tree) == 3);
    assert(tree == &n10.node);
    assert(n10.node.color == RB_BLACK);
    assert(n5.node.color == RB_BLACK);
    assert(n15.node.color == RB_BLACK);
    assert(n7.node.color == RB_RED);

    assert(my_rb_insert(&tree, &n3));

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
    struct mytype digits_ary[digits_ins_len];
    for (int i=0; i<digits_ins_len; i++) {
        digits_ary[i] = (struct mytype) { {0, NULL, {NULL}}, digits_ins[i] };
        assert(my_rb_insert(&digits, &digits_ary[i]));
    }
    assert(rbtree_validate(digits) == 3);

    digits = NULL;
    digits_ins = (uint32_t[]) {11,7,14,2,1,5,8,4};
    digits_ins_len = 8;
    for (int i=0; i<digits_ins_len; ++i) {
        digits_ary[i] = (struct mytype) { {0, NULL, {NULL}}, digits_ins[i] };
        assert(my_rb_insert(&digits, &digits_ary[i]));
    }
    assert(rbtree_validate(digits) == 3);


    return 0;
}
