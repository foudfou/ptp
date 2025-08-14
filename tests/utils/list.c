/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "utils/cont.h"
#include "utils/list.h"

/*  Typical object that can be added to a list. */
struct something {
    int val;
    struct list_item item;
};

int main ()
{
    /* Item declaration */
    struct something one = {1, LIST_ITEM_INIT(one.item)};
    assert(&one.item == one.item.prev);
    assert(&one.item == one.item.next);
    assert(list_is_empty(&one.item));

    /* List declaration and initialization */
    struct list_item list = LIST_ITEM_INIT(list);
    assert(&list == list.prev && list.prev == list.next);
    assert(list_is_empty(&list));
    list.next = &one.item; list.prev = &one.item;
    list_init(&list);
    assert(&list == list.prev && list.prev == list.next);

    /* List prepend */
    list_prepend(&list, &one.item);
    assert(list.next == &one.item);
    assert(list.prev == &one.item);
    assert(one.item.next == &list);
    assert(one.item.prev == &list);

    struct something two = {2, LIST_ITEM_INIT(two.item)};
    list_prepend(&list, &two.item);
    assert(list.next == &two.item);
    assert(list.prev == &one.item);
    assert(two.item.next == &one.item);
    assert(two.item.prev == &list);

    /* List append */
    struct something three = {3, LIST_ITEM_INIT(three.item)};
    list_append(&list, &three.item);
    assert(list.next == &two.item);
    assert(list.prev == &three.item);
    assert(one.item.next == &three.item);
    assert(three.item.prev == &one.item);
    assert(three.item.next == &list);
    struct something four = {4, LIST_ITEM_INIT(four.item)};
    list_append(&list, &four.item);

    /* List traversal */
    struct list_item * it = &list;
    int rule[4] = {2, 1, 3, 4};
    int i = 0;
    list_for(it, &list) {
        struct something* tmp = cont(it, struct something, item);
        assert(tmp);
        assert(rule[i] == tmp->val);
        i++;
    }
    assert(i == 4);
    i = 3;
    while ((it = it->prev) != &list) { // backward
        struct something* tmp = cont(it, struct something, item);
        assert(tmp);
        assert(rule[i] == tmp->val);
        i--;
    }
    assert(i == -1);

    /* List delete/remove/pop */
    const struct list_item * first = list.next;
    const struct list_item * second = list.next->next;
    list_delete_first(&list);
    assert(first == first->prev && first->prev == first->next);
    assert(list.next == second);

    list_delete(&one.item);
    assert(list.next == &three.item);
    list_delete(&three.item);
    list_delete(&four.item);
    assert(list_is_empty(&list));

    /* List concat */
    struct list_item l1 = LIST_ITEM_INIT(l1);
    list_append(&l1, &one.item);
    list_append(&l1, &two.item);
    struct list_item l2 = LIST_ITEM_INIT(l2);
    list_append(&l2, &three.item);
    list_append(&l2, &four.item);
    list_concat(&l1, &l2);

    assert(list_is_empty(&l2));
    assert(l1.prev == &four.item);
    it = &l1;
    rule[0] = 1; rule[1] = 2; rule[2] = 3; rule[3] = 4;
    i = 0;
    list_for(it, &l1) {
        struct something* tmp = cont(it, struct something, item);
        assert(tmp);
        assert(rule[i] == tmp->val);
        i++;
    }
    assert(i == 4);

    assert(list_is_empty(&list));
    list_concat(&list, &l1);
    assert(list_is_empty(&l1));
    i = 0;
    it = &list;
    list_for(it, &list) {
        struct something* tmp = cont(it, struct something, item);
        assert(tmp);
        assert(rule[i] == tmp->val);
        i++;
    }
    assert(i == 4);

    list_concat(&l1, &list);
    assert(list_is_empty(&list));
    i = 0;
    it = &l1;
    list_for(it, &l1) {
        struct something* tmp = cont(it, struct something, item);
        assert(tmp);
        assert(rule[i] == tmp->val);
        i++;
    }
    assert(i == 4);

    list_concat(&list, &l2);
    assert(list_is_empty(&list));
    assert(list_is_empty(&l2));

    list_delete(&four.item);
    list_append(&l2, &four.item);
    list_concat(&l1, &l2);
    assert(list_is_empty(&l2));
    i = 0;
    it = &l1;
    list_for(it, &l1) {
        struct something* tmp = cont(it, struct something, item);
        assert(tmp);
        assert(rule[i] == tmp->val);
        i++;
    }
    assert(i == 4);

    assert(list_count(&l1) == 4);


    return 0;
}
