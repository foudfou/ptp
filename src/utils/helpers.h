#ifndef HELPERS_H
#define HELPERS_H

#define list_free_all(itemp, type, field)                               \
    while (!list_is_empty(itemp)) {                                     \
        type *node = cont(itemp->prev, type, field);                    \
        list_delete(itemp->prev);                                       \
        free_safer(node);                                               \
    }

#endif /* HELPERS_H */
