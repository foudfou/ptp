/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "log.h"
#include "net/kad/bencode/parser.h"

#define POINTER_OFFSET(beg, end) (((end) - (beg)) / sizeof(*beg))


/* https://github.com/willemt/heapless-bencode/blob/master/bencode.c */
static bool benc_parse_int(struct benc_parser *p, struct benc_val *val)
{
    val->t = BENC_VAL_INT;
    long long val_tmp = val->i = 0;
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

        if (val_tmp < val->i) {
            sprintf(p->err_msg, "Overflow in int parsing at %zu.",
                    (size_t)POINTER_OFFSET(p->beg, p->cur));
            p->err = true;
            return false;
        }
        val->i = val_tmp;
        p->cur++;
    }
    while (*p->cur != 'e');
    p->cur++;  // eat up 'e'

    val->i *= sign;

    return true;
}

static bool benc_parse_str(struct benc_parser *p, struct benc_val *val)
{
    val->t = BENC_VAL_STR;
    val->s.len = 0;
    do {
        if (!isdigit(*p->cur)) {
            sprintf(p->err_msg, "Invalid character in rpc message at %zu.",
                    (size_t)POINTER_OFFSET(p->beg, p->cur));
            p->err = true;
            return false;
        }

        val->s.len *= 10;
        val->s.len += *p->cur - '0';
        p->cur++;
    }
    while (*p->cur != ':');

    if (val->s.len > BENC_PARSER_STR_LEN_MAX) {
            sprintf(p->err_msg, "String too long at %zu.",
                    (size_t)POINTER_OFFSET(p->beg, p->cur));
            p->err = true;
            return false;
    }

    p->cur++;
    memcpy(val->s.p, p->cur, val->s.len);
    p->cur += val->s.len;

    return true;
}

static void benc_parser_init(struct benc_parser *parser,
                             const char *buf, const size_t slen)
{
    memset(parser, 0, sizeof(*parser));
    parser->err = false;
    parser->cur = parser->beg = buf;
    parser->end = buf + slen;
}

static void benc_parser_terminate(struct benc_parser *parser)
{
    (void)parser; // FIXME:
}

static bool benc_stack_push(struct benc_parser *p, enum benc_mark tok)
{
    if (p->stack_off == BENC_PARSER_STACK_MAX) {
        p->err = true;
        strcpy(p->err_msg, "Reached maximum nested level.");
        return false;
    }
    p->stack[p->stack_off] = tok;
    p->stack_off++;
    return true;
}

static bool benc_stack_pop(struct benc_parser *p, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (p->stack_off == 0) {
            p->err = true;
            strcpy(p->err_msg, "Parser stack already empty.");
            return false;
        }
        p->stack_off--;
        p->stack[p->stack_off] = BENC_MARK_NONE;
    }
    return true;
}

void log_debug_val(const struct benc_val *val)
{
    if (val->t == BENC_VAL_INT)
        log_debug("dict_val int: %lld", val->i);
    else if (val->t == BENC_VAL_STR)
        log_debug("dict_val str: %.*s", val->s.len, val->s.p);
    else
        log_debug("dict_val unsupported: %d", val->t);
}

/*
  DICT: 'd' STR VAL 'e'
  LIST: 'l' VAL* 'e'
  VAL: STR | INT | DICT | LIST
 */
static bool benc_syntax_apply(struct benc_parser *p, struct benc_val *val,
                              const enum benc_mark got, enum benc_cont *emit)
{
    enum benc_mark mark = got;
    while (mark != BENC_MARK_NONE && p->stack_off > 0) {
        enum benc_mark stack_top = p->stack[p->stack_off-1];
        /* log_debug("mark=%u, stack_top=%u, stack_off=%zu", */
        /*           mark, stack_top, p->stack_off); */

        if (mark == BENC_MARK_VAL) {
            if (stack_top == BENC_MARK_LIST) {
                *emit = BENC_CONT_LIST_ELT;
            }
            else if (stack_top == BENC_MARK_DICT_VAL) {
                benc_stack_pop(p, 1);
                *emit = BENC_CONT_DICT_VAL;
                benc_stack_push(p, BENC_MARK_DICT_KEY);
            }
            else
                goto fail;
            mark = BENC_MARK_NONE;
        }

        else if (mark == BENC_MARK_INT) {
            mark = BENC_MARK_VAL;
        }

        else if (mark == BENC_MARK_STR) {
            if (stack_top == BENC_MARK_DICT_KEY) {
                benc_stack_pop(p, 1);
                *emit = BENC_CONT_DICT_KEY;
                benc_stack_push(p, BENC_MARK_DICT_VAL);
                mark = BENC_MARK_NONE;
            }
            else {
                mark = BENC_MARK_VAL;
            }
        }

        else if (mark == BENC_MARK_END) {
            if (stack_top == BENC_MARK_LIST) {
                benc_stack_pop(p, 1);
                val->t = BENC_VAL_LIST;
                *emit = BENC_CONT_LIST_END;
                mark = BENC_MARK_VAL;
            }
            else if (stack_top == BENC_MARK_DICT_KEY ||
                     stack_top == BENC_MARK_DICT_VAL) {
                benc_stack_pop(p, 1);
                val->t = BENC_VAL_LIST;
                *emit = BENC_CONT_DICT_END;
                mark = BENC_MARK_VAL;
            }
            else
                goto fail;
        }

        else
            goto fail;

        if (mark != BENC_MARK_NONE && *emit)
            log_debug("__FIXME: emit (%u) will be overridden.", *emit);
    }

    return true;

  fail:
    p->err = true;
    strcpy(p->err_msg, "Syntax error.");
    return false;
}

/* Bottom-up stream parsing: try to pull tokens one after one another, possibly
   creating nested collections. This happens in a 3 stage loop: low-level
   lexer, bencode syntax checking, and filling a struct like a kad message. */
bool benc_parse(void *object, benc_fill_fn benc_fill_object,
                const char buf[], const size_t slen)
{
    if (!slen) {
        log_error("Invalid void message.");
        return false;
    }

    bool ret = true;

    struct benc_parser parser;
    benc_parser_init(&parser, buf, slen);

    enum benc_mark mark = BENC_MARK_NONE;
    enum benc_cont emit = BENC_CONT_NONE;
    struct benc_val val;
    while (parser.cur != parser.end) {
        memset(&val, 0, sizeof(val));
        mark = BENC_MARK_NONE;
        emit = BENC_CONT_NONE;

        if (*parser.cur == 'i') { // int
            if (benc_parse_int(&parser, &val)) {
                mark = BENC_MARK_INT;
            }
        }

        else if (isdigit(*parser.cur)) { // string
            if (benc_parse_str(&parser, &val)) {
                mark = BENC_MARK_STR;
            }
        }

        else if (*parser.cur == 'l') { // list
            parser.cur++;
            if (benc_stack_push(&parser, BENC_MARK_LIST)) {
                emit = BENC_CONT_LIST_START;
                log_debug("LIST");
            }
        }

        else if (*parser.cur == 'd') { // dict
            parser.cur++;
            /* For dict we store the awaited state, starting with dict_key and
               alternating btw. dict_key/dict_val. */
            if (benc_stack_push(&parser, BENC_MARK_DICT_KEY)) {
                emit = BENC_CONT_DICT_START;
                log_debug("DICT");
            }
        }

        else if (*parser.cur == 'e') {
            parser.cur++;
            mark = BENC_MARK_END;
            log_debug("END");
        }

        else {
            parser.err = true;
            strcpy(parser.err_msg, "Syntax error."); // TODO: send reply
        }

        if (parser.err ||
            !benc_syntax_apply(&parser, &val, mark, &emit) ||
            !benc_fill_object(&parser, object, emit, &val)) {
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

  cleanup:
    benc_parser_terminate(&parser);

    return ret;
}
