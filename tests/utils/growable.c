/* Copyright (c) 2025 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include <stdio.h>
#include "log.h"
#include "utils/growable.h"

#define INT_CAP_LIMIT 100

GROWABLE_GENERATE(int_lst, int, 4, 2, INT_CAP_LIMIT)

struct item {
    int id;
    char data[64];
};

GROWABLE_GENERATE(test_item_lst, struct item, 2, 2, 50)

static void test_basic_operations(void)
{
    struct int_lst a = {0};

    // uninitialized state
    assert(a.len == 0);
    assert(a.cap == 0);
    assert(a.buf == NULL);

    // initialization
    assert(int_lst_init(&a, 8));
    assert(a.len == 0);
    assert(a.cap == 8);
    assert(a.buf != NULL);

    // append single item
    int item = 42;
    assert(int_lst_append(&a, &item, 1));
    assert(a.len == 1);
    assert(a.cap == 8);
    assert(a.buf[0] == 42);

    // append multiple items
    int items[] = {1, 2, 3};
    assert(int_lst_append(&a, items, 3));
    assert(a.len == 4);
    assert(a.cap == 8);
    assert(a.buf[1] == 1);
    assert(a.buf[2] == 2);
    assert(a.buf[3] == 3);

    // cleanup
    int_lst_reset(&a);
    assert(a.len == 0);
    assert(a.cap == 0);
    assert(a.buf == NULL);
}

static void test_growth_behavior(void)
{
    struct int_lst a = {0};

    // growth from uninitialized state
    int item = 1;
    assert(int_lst_append(&a, &item, 1));
    assert(a.len == 1);
    assert(a.cap == 4); // sz_init
    assert(a.buf[0] == 1);

    // fill to capacity
    int items[] = {2, 3, 4};
    assert(int_lst_append(&a, items, 3));
    assert(a.len == 4);
    assert(a.cap == 4);

    // trigger growth
    int more = 5;
    assert(int_lst_append(&a, &more, 1));
    assert(a.len == 5);
    assert(a.cap == 8); // factor

    // all data is preserved
    for (int i = 0; i < 5; i++) {
        assert(a.buf[i] == i + 1);
    }

    int_lst_reset(&a);
}

static void test_multiple_growths(void)
{
    struct int_lst a = {0};

    // multiple growth cycles
    for (int i = 0; i < 20; i++) {
        int value = i * 10;
        assert(int_lst_append(&a, &value, 1));
        assert(a.len == (size_t)(i + 1));
        assert(a.buf[i] == i * 10);
    }

    // cap: 4 -> 8 -> 16 -> 32
    assert(a.cap >= 20);

    // all data is preserved
    for (int i = 0; i < 20; i++) {
        assert(a.buf[i] == i * 10);
    }

    int_lst_reset(&a);
}

static void test_large_append(void)
{
    struct int_lst a = {0};

    // large array
    int large_data[50];
    for (int i = 0; i < 50; i++) {
        large_data[i] = i * i;
    }

    // append large chunk at once
    assert(int_lst_append(&a, large_data, 50));
    assert(a.len == 50);
    assert(a.cap >= 50);

    // data preserved
    for (int i = 0; i < 50; i++) {
        assert(a.buf[i] == i * i);
    }

    int_lst_reset(&a);
}

static void test_limit_enforcement(void)
{
    struct int_lst a = {0};

    assert(!int_lst_init(&a, INT_CAP_LIMIT + 1));

    assert(!int_lst_grow(&a, INT_CAP_LIMIT+50));
    assert(a.cap == 0 && a.len == 0);

    int large_data[INT_CAP_LIMIT+50];
    for (int i = 0; i < INT_CAP_LIMIT+50; i++)
        large_data[i] = i;

    assert(!int_lst_append(&a, large_data, INT_CAP_LIMIT+50));
    assert(a.len == 0);

    // small append ok
    for (int i = 0; i < 50; i++) {
        assert(int_lst_append(&a, &large_data[i], 1));
    }
    assert(a.len == 50);

    // large single appends may work. cap: 4 8 16 … 128
    int_lst_reset(&a);
    for (int i = 0; i < 128; i++)
        assert(int_lst_append(&a, &large_data[i], 1));
    // cap overflow
    assert(!int_lst_append(&a, &large_data[128], 1));

    int_lst_reset(&a);
}

static void test_boundary_conditions(void)
{
    struct int_lst a = {0};

    int item = 42;
    // zero-length append is nop
    assert(int_lst_append(&a, &item, 0));
    assert(a.len == 0);

    // single item
    assert(int_lst_append(&a, &item, 1));
    assert(a.len == 1);
    assert(a.buf[0] == 42);

    // reset on single item
    int_lst_reset(&a);
    assert(a.len == 0);
    assert(a.cap == 0);
    assert(a.buf == NULL);

    // init with minimal capacity
    assert(int_lst_init(&a, 1));
    assert(a.cap == 1);

    int_lst_reset(&a);
}

static void test_struct_growable(void)
{
    struct test_item_lst a = {0};

    struct item items[] = {
        {1, "first"},
        {2, "second"},
        {3, "third"}
    };

    // append
    assert(test_item_lst_append(&a, &items[0], 1));
    assert(a.len == 1);
    assert(a.cap == 2);
    assert(a.buf[0].id == 1);
    assert(strcmp(a.buf[0].data, "first") == 0);

    // append multiple
    assert(test_item_lst_append(&a, &items[1], 2));
    assert(a.len == 3);
    assert(a.buf[1].id == 2);
    assert(strcmp(a.buf[1].data, "second") == 0);
    assert(a.buf[2].id == 3);
    assert(strcmp(a.buf[2].data, "third") == 0);

    test_item_lst_reset(&a);
}

static void test_edge_case_limits(void)
{
    struct int_lst a = {0};

    // large single append that would exceed limit
    int large_data[INT_CAP_LIMIT+50]; // Larger than limit of 100
    for (int i = 0; i < INT_CAP_LIMIT+50; i++)
        large_data[i] = i;

    assert(!int_lst_append(&a, large_data, INT_CAP_LIMIT+50));
    assert(a.len == 0);

    // successful append within limit
    int reasonable_data[50];
    for (int i = 0; i < 50; i++)
        reasonable_data[i] = i;
    assert(int_lst_append(&a, reasonable_data, 50));
    assert(a.len == 50);

    int more_large_data[80];
    for (int i = 0; i < 80; i++)
        more_large_data[i] = i + 50;

    // cap limit to exceed
    assert(!int_lst_append(&a, more_large_data, 80));

    int_lst_reset(&a);
}

static void test_init_scenarios(void)
{
    struct int_lst a = {0};

    // init with zero capacity
    assert(int_lst_init(&a, 0));
    assert(a.cap == 0);
    assert(a.buf != NULL || a.cap == 0); // Implementation dependent

    int_lst_reset(&a);

    // init with large capacity
    assert(int_lst_init(&a, 64));
    assert(a.cap == 64);
    assert(a.len == 0);

    // buffer should be zeroed
    for (size_t i = 0; i < 64; i++) {
        assert(a.buf[i] == 0);
    }

    int_lst_reset(&a);
}

int main(void)
{
    assert(log_init(LOG_TYPE_STDOUT, LOG_UPTO(LOG_CRIT)));

    test_basic_operations();
    test_growth_behavior();
    test_multiple_growths();
    test_large_append();
    test_limit_enforcement();
    test_boundary_conditions();
    test_struct_growable();
    test_edge_case_limits();
    test_init_scenarios();

    log_shutdown(LOG_TYPE_STDOUT);
    return 0;
}
