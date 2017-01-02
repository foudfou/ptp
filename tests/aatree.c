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

/* For debugging, recover the definition from the SCM. */
extern void aatree_display_nodes(struct aatree_node *root);

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
