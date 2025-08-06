/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "utils/hash.h"

static const unsigned char HASH_SIZE     = 0xff;
static const unsigned int KEY_MAX_LENGTH = 1024;

/*  Define a hash type. */
struct stoi {
    struct list_item  item;
    const char       *key;
    int               val;
};

/* We MUST provide our own hash function. See hash.h.
   Here is djb2 reported by Dan Bernstein in comp.lang.c. */
static inline uint32_t stoi_hash(const char *str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c;
    return hash;
}

/* We MUST provide our own key comparison function. See hash.h. */
static inline int stoi_cmp(const char * keyA, const char * keyB)
{
    return strncmp(keyA, keyB, KEY_MAX_LENGTH);
}
HASH_GENERATE(stoi, stoi, item, key, char *, 0xff)


/* Here's how we create another hash type. */
struct person {
    struct list_item item;
    uint64_t key;
    // cppcheck-suppress unusedStructMember
    struct {char* name; int age; char gender;} val;
};

static inline uint32_t person_by_key_hash(const uint64_t key)
{
    return (uint32_t)key;
}

static inline int person_by_key_cmp(const uint64_t keyA, const uint64_t keyB)
{
    return keyB - keyA;
}
HASH_GENERATE(person_by_key, person, item, key, uint64_t, HASH_SIZE)


int main ()
{
    /* Hash declaration */
    struct list_item hash[HASH_SIZE];
    hash_init(hash, HASH_SIZE);
    assert(&hash[1] == hash[1].prev);
    assert(hash[1].prev == hash[1].next);

    /* Hash insertion */
    struct stoi one = {.key="one", .val=1, .item=LIST_ITEM_INIT(one.item)}; // static decl
    stoi_insert(hash, one.key, &(one.item));
    struct stoi bogus_one = {.key="one", .val=10, .item=LIST_ITEM_INIT(one.item)};
    // duplicate key not checked !!
    stoi_insert(hash, bogus_one.key, &(bogus_one.item));
    struct stoi two = {.key="two", .val=2, .item=LIST_ITEM_INIT(two.item)};
    stoi_insert(hash, two.key, &(two.item));

    /* Hash getting item */
    struct stoi * found = stoi_get(hash, "two");
    assert(!strncmp(found->key, "two", 3));
    assert(found->val == 2);
    struct stoi * notfound = stoi_get(hash, "three");
    assert(!notfound);

    /* Hash deletion */
    found = stoi_get(hash, "one");
    assert(found->val == 10);
    hash_delete(&found->item);
    found = stoi_get(hash, "one");
    assert(found->val == 1);

    /* Hash traversal */
    struct list_item* it = NULL;
    int values[2] = {0}; int j = 0;
    hash_for(hash, HASH_SIZE, it) {
        struct stoi* tmp = cont(it, struct stoi, item);
        assert(tmp);
        values[j++] = tmp->val;
    }
    int rule1[] = {2, 1}; int rule2[] = {1, 2};
    assert(!memcmp(values, rule1, 2*sizeof(int)) ||
           !memcmp(values, rule2, 2*sizeof(int)));


    /* Another hash */
    struct person bob = {LIST_ITEM_INIT(bob.item), 0xad00fe00, {"bob", 28, 'M'}};
    HASH_DECL(phone_book, HASH_SIZE);
    hash_init(phone_book, HASH_SIZE);
    person_by_key_insert(phone_book, bob.key, &(bob.item));
    struct person * someone = person_by_key_get(phone_book, 0xad00fe00);
    assert(!strncmp(someone->val.name, "bob", 3));


    return 0;
}
