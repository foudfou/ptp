#include <stdio.h>
#include <assert.h>
#include "../src/utils/cont.h"
#include "../src/utils/hash.h"

/*  Typical object that can be added to a list. */
struct entry {
    char* key;
    int value;
    struct list_item item;
};

int main ()
{
    /* Item declaration */
    struct entry one = {"one", 1, LIST_ITEM_DECL(one.item)};
    struct hash* hash = hash_create(100);
    hash_insert(hash, &one.item, (unsigned char *)one.key);

    struct entry two = {"two", 1, LIST_ITEM_DECL(two.item)};
    hash_insert(hash, &two.item, (unsigned char *)two.key);

    hash_destroy(hash);

    return 0;
}
