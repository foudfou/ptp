#include <assert.h>
#include <string.h>
#include "../src/utils/list.h"

static const unsigned char HASH_SIZE = 0xff;

/*  Define a hash type. */
#define HASH_NAME stoi
#define HASH_ENTRY_TYPE struct stoi
#define HASH_ENTRY_ITEM item
#define HASH_KEY_TYPE const char *
#define HASH_FUNCTION_PROVIDED
HASH_ENTRY_TYPE {
    HASH_KEY_TYPE key;
    int val;
    struct list_item HASH_ENTRY_ITEM;
};
#include "../src/utils/hash.h"

/* Provide our own hash function. We must:
   - name it according to hash.h conventions
   - make it return a uint32_t
   - define HASH_FUNCTION_PROVIDED
   Here is djb2 reported by Dan Bernstein in comp.lang.c */
static inline uint32_t hash_stoi_hash(const char* str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c;
    return hash;
}

/* Here's how we create another hash type. */
#define HASH_NAME person
#define HASH_ENTRY_TYPE struct person
#define HASH_ENTRY_ITEM item
#define HASH_KEY_TYPE uint64_t
HASH_ENTRY_TYPE {
    struct list_item HASH_ENTRY_ITEM;
    HASH_KEY_TYPE key;
    struct {char* name; int age; char gender;} val;
};
#include "../src/utils/hash.h"  // imported multiple times!


int main ()
{
    /* Hash declaration */
    struct list_item hash[HASH_SIZE];
    hash_init(hash, HASH_SIZE);
    assert(&hash[1] == hash[1].prev);
    assert(hash[1].prev == hash[1].next);

    /* Hash insertion */
    struct stoi one = {"one", 1, LIST_ITEM_DECL(one.item)}; // static decl
    hash_stoi_insert(hash, HASH_SIZE, one.key, &one.item);
    struct stoi bogus_one = {"one", 10, LIST_ITEM_DECL(one.item)};
    // duplicate key not checked !!
    hash_stoi_insert(hash, HASH_SIZE, bogus_one.key, &bogus_one.item);
    struct stoi two = {"two", 2, LIST_ITEM_DECL(two.item)};
    hash_stoi_insert(hash, HASH_SIZE, two.key, &two.item);

    /* Hash getting item */
    struct stoi * found = hash_stoi_get(hash, HASH_SIZE, "two");
    assert(!strncmp(found->key, "two", 3));
    assert(found->val == 2);
    struct stoi * notfound = hash_stoi_get(hash, HASH_SIZE, "three");
    assert(!notfound);

    /* Hash deletion */
    found = hash_stoi_get(hash, HASH_SIZE, "one");
    assert(found->val == 10);
    hash_delete(&found->item);
    found = hash_stoi_get(hash, HASH_SIZE, "one");
    assert(found->val == 1);

    /* Hash traversal */
    int i = 0; struct list_item* it = hash;
    int values[2] = {0}; int j = 0;
    hash_for_all(hash, HASH_SIZE, i, it) {
        struct stoi* tmp = cont(it, struct stoi, item);
        values[j++] = tmp->val;
    }
    int rule1[] = {2, 1}; int rule2[] = {1, 2};
    assert(!memcmp(values, rule1, 2*sizeof(int)) ||
           !memcmp(values, rule2, 2*sizeof(int)));


    /* Another hash */
    struct person bob = {LIST_ITEM_DECL(bob.item), 0xad00fe00, {"bob", 28, 'M'}};
    DECLARE_HASHTABLE(phone_book, HASH_SIZE);
    hash_init(phone_book, HASH_SIZE);
    hash_person_insert(phone_book, HASH_SIZE, bob.key, &bob.item);
    struct person * someone = hash_person_get(phone_book, HASH_SIZE, 0xad00fe00);
    assert(!strncmp(someone->val.name, "bob", 3));


    return 0;
}
