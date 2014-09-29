#ifndef LIST_H
#define LIST_H

/**
 * A Linux-style circular list where the data *contains* the list.
 *
 * Also defines a list where @next is the first element and @prev the last.
 */
struct list_item {
    struct list_item * prev;
    struct list_item * next;
};

#define LIST_ITEM_DECL(name) { &(name), &(name) }

/**
 * Initialize a list or an item.
 *
 * Lists and items are arbitrariliy initialized by making their prev and next
 * pointing to themselves.
 */
static inline void list_init(struct list_item* item) {
    item->next = item;
    item->prev = item;
}

/**
 * Insert an item between item @at and the previous one.
 */
static inline void list_insert(struct list_item* at, struct list_item* new)
{
    at->prev->next = new;
    new->prev = at->prev;
    new->next = at;
    at->prev = new;
}

/**
 * Insert an item at the beginning of a list.
 */
static inline void list_prepend(struct list_item* list, struct list_item* item) {
    list_insert(list->next, item);
    /* list->next->prev = item; */
    /* item->next = list->next; */
    /* item->prev = list; */
    /* list->next = item; */
}

/**
 * Insert an item at the end of a list.
 */
static inline void list_append(struct list_item* list, struct list_item* item) {
    list_insert(list, item);
    /* list->prev->next = item; */
    /* item->prev = list->prev; */
    /* item->next = list; */
    /* list->prev = item; */
}

/**
 * Remove the item @at from a list and return it.
 */
static inline struct list_item* list_delete(struct list_item* list,
                                            struct list_item* at)
{
    at->next->prev = list;
    list->next = at->next;
    list_init(at);
    return at;
}

/**
 * Remove the first item from a list and return it.
 */
static inline struct list_item* list_delete_first(struct list_item* list)
{
    return list_delete(list, list->next);
    /* struct list_item* first = list->next; */
    /* first->next->prev = list; */
    /* list->next = first->next; */
    /* list_init(first); */
    /* return first; */
}

/**
 * Remove the last item from a list and return it.
 */
static inline struct list_item* list_delete_last(struct list_item* list)
{
    return list_delete(list, list->prev);
    /* struct list_item* last = list->prev; */
    /* last->prev->next = list; */
    /* list->prev = last->prev; */
    /* list_init(last); */
    /* return last; */
}

/**
 * Iterate over a list
 */
#define list_for(it, list) \
    while ((it = it->next) != (list))

#endif /* LIST_H */
