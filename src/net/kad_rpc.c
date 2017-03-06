/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "log.h"
#include "net/kad_rpc.h"

/**
 * "t" transaction id: 2 chars.
 * "y" message type: "q" for query, "r" for response, or "e" for error.
 *
 * "q" query method name: str.
 * "a" named arguments: dict.
 *
 * "r" named return values: dict.
 *
 * "e" list: error code (int), error msg (str).
 */

#define POINTER_OFFSET(pbeg, pend) (((pend) - (pbeg)) / sizeof(*pbeg))

union kad_rpc_val {
    long long i;
    struct {
        char p[KAD_RPC_PARSER_STR_MAX]; /* TODO: use an iobuf ? */
        size_t len;
    } s;
};


/* https://github.com/willemt/heapless-bencode/blob/master/bencode.c */
static bool
kad_rpc_parse_int(struct kad_rpc_parser *p, long long *val)
{
    long long val_tmp = *val = 0;
    int sign = 1;

    p->cur++;  // eat up 'i'
    if (*p->cur == '-') {
        sign = -1;
        p->cur++;
    }

    do {
        if (!isdigit(*p->cur)) {
            sprintf(p->err_msg, "Invalid character in rpc message at %zu.",
                    (size_t)POINTER_OFFSET(p->beg, p->cur));
            p->err = true;
            return false;
        }

        val_tmp *= 10;
        val_tmp += *p->cur - '0';

        if (val_tmp < *val) {
            sprintf(p->err_msg, "Overflow in int parsing at %zu.",
                    (size_t)POINTER_OFFSET(p->beg, p->cur));
            p->err = true;
            return false;
        }
        *val = val_tmp;
        p->cur++;
    }
    while (*p->cur != 'e');
    p->cur++;  // eat up 'e'

    *val *= sign;

    return true;
}

static bool
kad_rpc_parse_str(struct kad_rpc_parser *p, char *str, size_t *len)
{
    *len = 0;
    do {
        if (!isdigit(*p->cur)) {
            sprintf(p->err_msg, "Invalid character in rpc message at %zu.",
                    (size_t)POINTER_OFFSET(p->beg, p->cur));
            p->err = true;
            return false;
        }

        *len *= 10;
        *len += *p->cur - '0';
        p->cur++;
    }
    while (*p->cur != ':');

    if (*len > KAD_RPC_PARSER_STR_MAX) {
            sprintf(p->err_msg, "String too long at %zu.",
                    (size_t)POINTER_OFFSET(p->beg, p->cur));
            p->err = true;
            return false;
    }

    p->cur++;
    memcpy(str, p->cur, *len);
    p->cur += *len;

    return true;
}

static void kad_rpc_parser_init(struct kad_rpc_parser *parser,
                                const char *buf, const size_t slen)
{
    memset(parser, 0, sizeof(*parser));
    parser->err = false;
    parser->cur = parser->beg = buf;
    parser->end = buf + slen;
}

static void kad_rpc_parser_terminate(struct kad_rpc_parser *parser)
{
/* TODO: */
}

static bool
kad_rpc_parser_push(struct kad_rpc_parser *p, enum kad_rpc_stack tok)
{
    if (p->stack_off == KAD_RPC_PARSER_STACK_MAX) {
        p->err = true;
        strcpy(p->err_msg, "Reached maximum nested level.");
        return false;
    }
    p->stack[p->stack_off] = tok;
    p->stack_off++;
    return true;
}

static bool
kad_rpc_parser_pop(struct kad_rpc_parser *p, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (p->stack_off == 0) {
            p->err = true;
            strcpy(p->err_msg, "Parser stack already empty.");
            return false;
        }
        p->stack_off--;
        p->stack[p->stack_off] = KAD_RPC_STACK_NONE;
    }
    return true;
}

static bool kad_rpc_msg_store(struct kad_rpc_parser *p, union kad_rpc_val *val)
{
    return true;                // TODO:
}

/*
  DICT: 'd' STR VAL 'e'
  LIST: 'l' VAL* 'e'
  VAL: STR | INT | DICT | LIST
 */
static bool
kad_rpc_parser_check_state(struct kad_rpc_parser *p, enum kad_rpc_stack got,
                           union kad_rpc_val *val)
{
    enum kad_rpc_stack next = got;
    while (next != KAD_RPC_STACK_NONE && p->stack_off > 0) {
        enum kad_rpc_stack cur = p->stack[p->stack_off-1];
        log_debug("next=%u, cur=%u, stack_off=%zu", next, cur, p->stack_off);

        if (next == KAD_RPC_STACK_VAL) {
            next = KAD_RPC_STACK_NONE;

            if (cur == KAD_RPC_STACK_DICT_VAL) {
                kad_rpc_parser_pop(p, 1);
                kad_rpc_parser_push(p, KAD_RPC_STACK_DICT_KEY);
            }
            else if (cur == KAD_RPC_STACK_LIST) {
            }
            else
                goto fail;
        }

        else if (next == KAD_RPC_STACK_INT) {
            next = KAD_RPC_STACK_VAL;
        }

        else if (next == KAD_RPC_STACK_STR) {
            if (cur == KAD_RPC_STACK_DICT_KEY) {
                kad_rpc_parser_pop(p, 1);
                kad_rpc_parser_push(p, KAD_RPC_STACK_DICT_VAL);
                kad_rpc_msg_store(p, val);
                next = KAD_RPC_STACK_NONE;
            }
            else {
                next = KAD_RPC_STACK_VAL;
            }
        }

        else if (next == KAD_RPC_STACK_END) {
            if (cur == KAD_RPC_STACK_LIST ||
                cur == KAD_RPC_STACK_DICT_KEY) {
                kad_rpc_parser_pop(p, 1);
                next = KAD_RPC_STACK_VAL;
            }
            else
                goto fail;
        }

        else
            goto fail;
    }

    return true;

  fail:
    p->err = true;
    strcpy(p->err_msg, "Syntax error.");
    return false;
}

bool kad_rpc_parse(const char host[], const char service[],
                   const char buf[], const size_t slen)
{
    bool ret = true;

    struct kad_rpc_parser parser;
    kad_rpc_parser_init(&parser, buf, slen);

    enum kad_rpc_stack evt = KAD_RPC_STACK_NONE;
    union kad_rpc_val val;
    while (parser.cur != parser.end) {
        memset(&val, 0, sizeof(val));

        if (*parser.cur == 'i') { // int
            if (kad_rpc_parse_int(&parser, &val.i)) {
                evt = KAD_RPC_STACK_INT;
                log_debug("INT: %lld", val.i);
            }
        }

        else if (isdigit(*parser.cur)) { // string
            if (kad_rpc_parse_str(&parser, val.s.p, &val.s.len)) {
                evt = KAD_RPC_STACK_STR;
                log_debug("STR: %.*s", val.s.len, val.s.p);
            }
        }

        else if (*parser.cur == 'l') { // list
            parser.cur++;
            if (kad_rpc_parser_push(&parser, KAD_RPC_STACK_LIST)) {
                evt = KAD_RPC_STACK_NONE;
                log_debug("LIST");
            }
        }

        else if (*parser.cur == 'd') { // dict
            parser.cur++;
            if (kad_rpc_parser_push(&parser, KAD_RPC_STACK_DICT_KEY)) {
                evt = KAD_RPC_STACK_NONE;
                log_debug("DICT");
            }
        }

        else if (*parser.cur == 'e') {
            parser.cur++;
            evt = KAD_RPC_STACK_END;
            log_debug("END");
        }

        else {
            parser.err = true;
            strcpy(parser.err_msg, "Syntax error."); // TODO: send reply
        }

        if (parser.err ||
            !kad_rpc_parser_check_state(&parser, evt, &val)) {
            log_error(parser.err_msg);  // TODO: send reply
            ret = false;
            goto cleanup;
        }
    }

    if (parser.stack_off > 0) {
        log_error("Invalid input: unclosed containers.");
        ret = false;
        goto cleanup;
    }
    log_debug("__PARSE SUCCESSFUL__");
  cleanup:
    kad_rpc_parser_terminate(&parser);

    return ret;
}
