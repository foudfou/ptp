/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include <string.h>
#include "utils/safer.h"
#include "utils/heap.h"

static inline int int_heap_cmp(const int a, const int b)
{
    return a - b;
}
HEAP_GENERATE(int_heap, int)


struct some {
    char c;
};

static inline int min_heap_cmp(const struct some *a, const struct some *b)
{
    return b->c - a->c;
}
HEAP_GENERATE(min_heap, struct some *)
HEAP_GENERATE_REPLACE_TOP(min_heap, struct some *)

#define N 10


int main ()
{
    struct int_heap ints = {0};
    assert(int_heap_init(&ints, 4));
    assert(memcmp(ints.items, (int[N]){0}, N) == 0);

    for (size_t i=0; i<N; ++i)
        assert(int_heap_push(&ints, i));

    /*
      |         9
      |     8       5
      |   6   7   1   4
      |  0 3 2
     */
    int expect_ints[N] = {9,8,5,6,7,1,4,0,3,2};
    assert(ints.len == N);
    assert(memcmp(ints.items, expect_ints, N) == 0);

    for (size_t i=0; i<N; ++i)
        assert(int_heap_pop(&ints) == N - 1 - (int)i);
    assert(memcmp(ints.items, (int[N]){0}, N) == 0);

    free_safer(ints.items);


    struct min_heap somes = {0};
    assert(min_heap_init(&somes, 4));
    assert(memcmp(somes.items, (int[N]){0}, N) == 0);

    struct some have[N] = {{74},{73},{72},{71},{70},{69},{68},{67},{66},{65}};

    assert(min_heap_push(&somes, &have[2]));
    assert(min_heap_push(&somes, &have[1]));
    assert(min_heap_push(&somes, &have[0]));
    assert(min_heap_pop(&somes) == &have[2]);
    assert(min_heap_pop(&somes) == &have[1]);
    assert(min_heap_pop(&somes) == &have[0]);
    assert(somes.len == 0);
    assert(min_heap_pop(&somes) == NULL);

    for (size_t i=0; i<N; ++i)
        assert(min_heap_push(&somes, &have[i]));
    assert(somes.len == N);

    /*
      |            65
      |      66         69
      |   68    67    73  70
      | 74 71  72
     */
    struct some expect_some[N] = {{65},{66},{69},{68},{67},{73},{70},{74},{71},{72}};
    for (size_t i=0; i<N; ++i) {
        /* printf("heap[%zu] -> %d; underlying[%zu]=%d\n", i, somes.items[i]->c, i, have[i].c); */
        assert(somes.items[i]->c == expect_some[i].c);
    }

    // replace_top
    struct some *min = HEAP_PEEK(somes);
    min->c = 75;
    min_heap_replace_top(&somes, min);
    /*
      |            66
      |      67          69
      |   68     72    73  70
      | 74  71 75
    */
    struct some expect_some2[] = {{66},{67},{69},{68},{72},{73},{70},{74},{71},{75}};
    for (size_t i=0; i<N; ++i) {
        /* printf("heap[%zu] -> %d; underlying[%zu]=%d\n", i, somes.items[i]->c, i, have[i].c); */
        assert(somes.items[i]->c == expect_some2[i].c);
    }


    struct some expect_pop[N] = {{66},{67},{68},{69},{70},{71},{72},{73},{74},{75}};
    for (size_t i=0; i<N; ++i){
        struct some *got = min_heap_pop(&somes);
        assert(got->c == expect_pop[(int)i].c);
    }
    assert(memcmp(somes.items, (int[N]){0}, N) == 0);


    free_safer(somes.items);


    return 0;
}
