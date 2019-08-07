/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#ifndef PARSER_H
#define PARSER_H

#include <stddef.h>
#include <stdbool.h>

#define BENC_PARSER_STACK_MAX   32
#define BENC_PARSER_STR_LEN_MAX 256

enum benc_mark {
    BENC_MARK_NONE,
    BENC_MARK_VAL,           /* 1 */
    BENC_MARK_INT,
    BENC_MARK_STR,
    BENC_MARK_LIST,          /* 4 */
    BENC_MARK_DICT_KEY,      /* 5 */
    BENC_MARK_DICT_VAL,      /* 6 */
    BENC_MARK_END,
};

enum benc_cont {
    BENC_CONT_NONE,
    BENC_CONT_DICT_START,
    BENC_CONT_DICT_KEY,
    BENC_CONT_DICT_VAL,
    BENC_CONT_DICT_END,
    BENC_CONT_LIST_START,
    BENC_CONT_LIST_ELT,
    BENC_CONT_LIST_END ,
};

struct benc_parser {
    const char     *beg;        /* pointer to begin of buffer */
    const char     *cur;        /* pointer to current char in buffer */
    const char     *end;        /* pointer to end of buffer */
    bool            err;
    char            err_msg[BENC_PARSER_STR_LEN_MAX];
    enum benc_mark  stack[BENC_PARSER_STACK_MAX];
    size_t          stack_off;
    int             msg_field;
};

enum benc_val_type {
    BENC_VAL_NONE,
    BENC_VAL_INT,
    BENC_VAL_STR,
    BENC_VAL_LIST,
    BENC_VAL_DICT,
};

struct benc_val {
    enum benc_val_type t;
    union {
        long long      i;
        struct {
            /* TODO: use an iobuf instead ? */
            char       p[BENC_PARSER_STR_LEN_MAX];
            size_t     len;
        } s;
    };
};

typedef bool (*benc_fill_fn)(
    struct benc_parser    *p,
    void                  *object, // struct kad_rpc_msg *msg
    const enum benc_cont   emit,
    const struct benc_val *val
);

bool benc_parse(void *object, benc_fill_fn benc_fill_object,
                const char buf[], const size_t slen);

#endif /* PARSER_H */
