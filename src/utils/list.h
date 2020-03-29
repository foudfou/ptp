/* Copyright (c) 2014 Foudil Brétel.  All rights reserved. */
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

#define LIST_ITEM_INIT(name) { &(name), &(name) }

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
 * Test wether a list is empty.
 */
static inline int list_is_empty(struct list_item* list)
{
    return list->next == list;
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
static inline void list_prepend(struct list_item* list, struct list_item* item)
{
    list_insert(list->next, item);
}

/**
 * Insert an item at the end of a list.
 */
static inline void list_append(struct list_item* list, struct list_item* item)
{
    list_insert(list, item);
}

/**
 * Remove the item @at from a list and return it.
 */
static inline void list_delete(struct list_item* item)
{
    item->next->prev = item->prev;
    item->prev->next = item->next;
    list_init(item);
}

/**
 * Remove the first item from a list and return it.
 */
static inline void list_delete_first(struct list_item* list)
{
    list_delete(list->next);
}

/**
 * Remove the last item from a list and return it.
 */
static inline void list_delete_last(struct list_item* list)
{
    list_delete(list->prev);
}

/**
 * Concatenate two lists. The second list is truncated.
 */
static inline void list_concat(struct list_item* l1, struct list_item* l2)
{
    l2->next->prev = l1->prev;
    if (l2->next->next == l2) // l2->next is last
        l2->next->next = l1;
    l1->prev->next = l2->next;
    l2->prev->next = l1;
    l1->prev = l2->prev;
    list_init(l2);
}

/**
 * Iterate forward over a list.
 *
 * CAUTION: You need to `it = it->prev` before `list_delete`ing inside
 * `list_for`, otherwise the loop will loop endlessly.
 */
#define list_for(it, list) \
    while ((it = it->next) != (list))

/**
 * Count number of elements.
 */
static inline int list_count(struct list_item* list)
{
    int len = 0;
    struct list_item *it = list;
    list_for(it, list)
        len++;
    return len;
}

#endif /* LIST_H */
