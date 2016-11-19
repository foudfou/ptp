/* FIXME: insert, search, … shoulb be straight macros like:
 * BTREE_INSERT(name, type, …). See freebsd/sys/tree.h */

#ifdef BTREE_CREATE

#ifndef BTREE_NAME
    #error must give this map type a name by defining BTREE_NAME
#endif
#ifndef BTREE_TYPE
    #error must define BTREE_TYPE
#endif
#ifndef BTREE_NODE_TYPE
    #error must define BTREE_NODE_TYPE
#endif
#ifndef BTREE_NODE_MEMBER
    #error must define BTREE_NODE_MEMBER
#endif
#if defined(BTREE_KEY_TYPE)
    assert_compilation(!(sizeof(BTREE_KEY_TYPE) % 4));
#else
    #error must define BTREE_KEY_TYPE
#endif
#ifndef BTREE_KEY_MEMBER
    #error must define BTREE_KEY_MEMBER
#endif

#define BTREE_FUNCTION(name)                                            \
    BTREE_GLUE(                                                         \
        BTREE_GLUE(                                                     \
            BTREE_GLUE(                                                 \
                BTREE_GLUE(BTREE_NODE_NAME, _), BTREE_NAME), _), name)
#define BTREE_GLUE(x, y) BTREE_GLUE2(x, y)
#define BTREE_GLUE2(x, y) x ## y

#define __BTREE_LINK_NODE BTREE_GLUE(BTREE_NODE_NAME, _link_node)

/**
 * Insert a node into a tree.
 */
static inline bool
BTREE_FUNCTION(insert)(BTREE_NODE_TYPE **tree, BTREE_TYPE *data)
{
    /* We'll iterate on the *link* fields, which enables us to look for the
       next node while keeping a hold on the current node. Hence the double
       pointer. */
    BTREE_NODE_TYPE **it = tree, *parent = NULL;

    while (*it) {
        BTREE_TYPE *this = cont(*it, BTREE_TYPE, BTREE_NODE_MEMBER);
        int result = BTREE_FUNCTION(compare)(data->BTREE_KEY_MEMBER,
                                             this->BTREE_KEY_MEMBER);

        if (result == 0)
            return false;       // already there
        else {
            parent = *it;
            it = &((*it)->link[RIGHT_IF(result > 0)]);
        }
    }

    __BTREE_LINK_NODE(&data->BTREE_NODE_MEMBER, parent, it);

    return true;
}

/**
 * Gets an entry by its key.
 *
 * Returns NULL when not found.
 */
static inline BTREE_TYPE*
BTREE_FUNCTION(search)(BTREE_NODE_TYPE *tree, BTREE_KEY_TYPE key)
{
    BTREE_NODE_TYPE *it = tree;

    while (it) {
        BTREE_TYPE *data = cont(it, BTREE_TYPE, BTREE_NODE_MEMBER);
        int result = BTREE_FUNCTION(compare)(key, data->BTREE_KEY_MEMBER);

        if (result == 0)
            return data;
        else
            it = it->link[RIGHT_IF(result > 0)];
    }
    return NULL;
}


#undef BTREE_NODE_MEMBER
#undef BTREE_FUNCTION
#undef BTREE_KEY_TYPE
#undef BTREE_KEY_MEMBER
#undef BTREE_TYPE
#undef BTREE_NAME
#undef BTREE_CREATE

#endif // defined BTREE_CREATE
