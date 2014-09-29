/* #include <stdio.h> */
#include <assert.h>
#include "../src/utils/cont.h"
#include "../src/utils/list.h"

/*  Typical object that can be added to a list. */
struct something {
    int value;
    struct list_item item;
};

int main ()
{
    /* Item declaration */
    struct something one = {1, LIST_ITEM_DECL(one.item)};
    assert(&one.item == one.item.prev);
    assert(&one.item == one.item.next);

    /* List declaration and initialization */
    struct list_item list = LIST_ITEM_DECL(list);
    assert(&list == list.prev && list.prev == list.next);
    list.next = &one.item; list.prev = &one.item;
    list_init(&list);
    assert(&list == list.prev && list.prev == list.next);

    /* List prepend */
    list_prepend(&list, &one.item);
    assert(list.next == &one.item);
    assert(list.prev == &one.item);
    assert(one.item.next == &list);
    assert(one.item.prev == &list);

    struct something two = {2, LIST_ITEM_DECL(two.item)};
    list_prepend(&list, &two.item);
    assert(list.next == &two.item);
    assert(list.prev == &one.item);
    assert(two.item.next == &one.item);
    assert(two.item.prev == &list);

    /* List append */
    struct something three = {3, LIST_ITEM_DECL(three.item)};
    list_append(&list, &three.item);
    assert(list.next == &two.item);
    assert(list.prev == &three.item);
    assert(one.item.next == &three.item);
    assert(three.item.prev == &one.item);
    assert(three.item.next == &list);
    struct something four = {4, LIST_ITEM_DECL(four.item)};
    list_append(&list, &four.item);

    /* List traversal */
    struct list_item * it = &list;
    int rule[4] = {2, 1, 3, 4};
    int i = 0;
    list_for(it, &list) {
        struct something* tmp = cont(it, struct something, item);
        assert(rule[i] == tmp->value);
        i++;
    }
    i = 3;
    while ((it = it->prev) != &list) { // backward
        struct something* tmp = cont(it, struct something, item);
        assert(rule[i] == tmp->value);
        i--;
    }

    /* List delete/remove/pop */
    struct list_item * first = list.next;
    struct list_item * second = list.next->next;
    struct list_item * item = list_delete_first(&list);
    assert(item == item->prev && item->prev == item->next);
    assert(item == first);
    assert(list.next == second);

    // FIXME: mem cleaning
    /* pp_list_erase (&list, &that.item); */
    /* pp_list_erase (&list, &other.item); */
    /* pp_list_item_term (&that.item); */
    /* pp_list_item_term (&other.item); */
    /* pp_list_term (&list); */

    return 0;
}
