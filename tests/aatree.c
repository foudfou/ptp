#include <assert.h>
#include <stdlib.h>
#include <time.h>
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

#include <stdio.h>
void aatree_display_nodes(struct aatree_node *root)
{
    if (!root)
        return;

    struct aatree_node *ln = root->link[LEFT];
    struct aatree_node *rn = root->link[RIGHT];

    struct foo *this = cont(root, struct foo, node);
    if (ln) {
        struct foo *lobj = cont(ln, struct foo, node);
        printf("\"%d(%d)\" -> \"%d(%d)\";\n", this->key, root->level,
               lobj->key, ln->level);
    }
    if (rn) {
        struct foo *robj = cont(rn, struct foo, node);
        printf("\"%d(%d)\" -> \"%d(%d)\";\n", this->key, root->level,
               robj->key, rn->level);
    }

    aatree_display_nodes(ln);
    aatree_display_nodes(rn);
}

/* Graphviz: `dot -Tjpg -O /tmp/aa2.dot`, or better
   `dot /tmp/a1.dot | gvpr -c -ftools/tree.gv | neato -n -Tpng -o /tmp/a1.png` */
void aatree_display(struct aatree_node *root) {
    printf("digraph aatree {\n");
    aatree_display_nodes(root);
    printf("}\n");
}

static inline bool
foo_delete(struct aatree_node **tree, struct aatree_node *node)
{
    struct aatree_node **parent_link_orig = aatree_parent_link(tree, node);
    struct aatree_node *parent_orig = node->parent;
    int level_orig = node->level;

    if (!foo_bs_delete(tree, node))
        return false;

    /* Test if delete-by-swap occured. */
    if (*parent_link_orig && node->parent != parent_orig)
        (*parent_link_orig)->level = level_orig;

    while (node) {
        unsigned int levell = node->link[LEFT] ? node->link[LEFT]->level : 0;
        unsigned int levelr = node->link[RIGHT] ? node->link[RIGHT]->level : 0;
        if ((levell < node->level - 1) || (levelr < node->level - 1)) {
            node->level -= 1;
            if (levelr > node->level)
                node->link[RIGHT]->level = node->level;

            struct aatree_node **top = aatree_parent_link(tree, node);
            aatree_skew(node, top);
            if ((*top)->link[RIGHT])
                aatree_skew((*top)->link[RIGHT], &((*top)->link[RIGHT]));
            if ((*top)->link[RIGHT]->link[RIGHT])
                aatree_skew((*top)->link[RIGHT]->link[RIGHT],
                            &((*top)->link[RIGHT]->link[RIGHT]));
            aatree_split((*top), top);
            aatree_split((*top)->link[RIGHT], &((*top)->link[RIGHT]));
        }

        node = node->parent;
    }

    return true;
}

bool aatree_validate(struct aatree_node *root)
{
    if (!root)
        return true;

    struct aatree_node *ln = root->link[LEFT];
    struct aatree_node *rn = root->link[RIGHT];

    aatree_validate(ln);
    aatree_validate(rn);

    /* Left child not same level. */
    if (ln)
        assert(root->level == (ln->level + 1));

    /* No more than two right children with the same level. */
    if (rn && rn->link[RIGHT])
        assert(root->level != rn->link[RIGHT]->level);

    /* Binary tree violation / Invalid binary search tree */
    uint32_t root_key = cont(root, struct foo, node)->key;
    assert(!((ln && cont(ln, struct foo, node)->key >= root_key) ||
             (rn && cont(rn, struct foo, node)->key <= root_key)));

    /* We shouldn't need to count pseudo-heights if the previous constraints
       are verified. So the level field should be accurate. */
    return true;
}

int main ()
{
    int n1len = 14;
    struct foo n1[n1len];
    for (int i = 0; i < n1len; i++)
        n1[i] = (struct foo) FOO_INIT(0, NULL, NULL, NULL, i+1);

    /* Btree declaration */
    AATREE_DECL(aa1);
    assert(!aa1);
    assert(aatree_is_empty(aa1));

    /* Tree insertion */
    for (int i = 0; i < n1len; i++) {
        AATREE_NODE_INIT(n1[i].node);
        assert(foo_insert(&aa1, &n1[i]));
    }
    aatree_validate(aa1);

    srand(time(NULL));  // once
    int n2len = 132;
    struct foo n2[n2len];
    AATREE_DECL(aa2);
    for (int i = 0; i < n2len; i++) {
        bool inserted = false;
        while (!inserted) {
            n2[i] = (struct foo) FOO_INIT(0, NULL, NULL, NULL, rand() % 256);
            AATREE_NODE_INIT(n2[i].node);
            inserted = foo_insert(&aa2, &n2[i]);
        }
    }
    aatree_validate(aa2);

    for (int i = 0; i < n2len; i++) {
        assert(foo_delete(&aa2, &(n2[i].node)));
        aatree_validate(aa2);
    }


    return 0;
}
