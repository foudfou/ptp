/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "log.h"
#include "net/kad_rpc.h"

#define KAD_RPC_PARSER_STACK_MAX    32
#define KAD_RPC_PARSER_ERR_MSG_MAX 256
#define KAD_RPC_PARSER_STR_MAX     256

#define POINTER_OFFSET(pbeg, pend) (((pend) - (pbeg)) / sizeof(*pbeg))

enum kad_rpc_type {
    KAD_RPC_TYPE_NONE,
    KAD_RPC_TYPE_ERROR,
    KAD_RPC_TYPE_QUERY,
    KAD_RPC_TYPE_RESPONSE,
};

enum kad_rpc_meth {
    KAD_RPC_METH_NONE,
    KAD_RPC_METH_PING,
    KAD_RPC_METH_FIND_NODE,
};

struct krpc_node_info {
    kad_guid id;
    char     host[NI_MAXHOST];
    char     service[NI_MAXSERV];
};

/**
 * Naive flattened dictionary for all possible messages.
 *
 * The protocol being relatively tiny, data size considered limited (a Kad
 * message must fit into an UDP buffer: no application flow control), every
 * awaited value should fit into well-defined fields. So we don't bother to
 * serialize lists and dicts for now.
 */
struct kad_rpc_msg {
    char                  tx_id[2]; /* t */
    kad_guid              node_id;  /* from {a,r} dict: id_str */
    enum kad_rpc_type     type;     /* y {q,r,e} */
    unsigned long long    err_code;
    struct iobuf          err_msg;
    enum kad_rpc_type     meth;     /* q {"ping","find_node"} */
    kad_guid              target;   /* from {a,r} dict: target, nodes */
    struct krpc_node_info nodes[KAD_K_CONST]; /* from {a,r} dict: target, nodes */
};

enum kad_rpc_mark {
    KAD_RPC_MARK_NONE,
    KAD_RPC_MARK_VAL,           /* 1 */
    KAD_RPC_MARK_INT,
    KAD_RPC_MARK_STR,
    KAD_RPC_MARK_LIST,          /* 4 */
    KAD_RPC_MARK_DICT_KEY,      /* 5 */
    KAD_RPC_MARK_DICT_VAL,      /* 6 */
    KAD_RPC_MARK_END,
};

enum kad_rpc_cont {
    KAD_RPC_CONT_NONE,
    KAD_RPC_CONT_DICT_START,
    KAD_RPC_CONT_DICT_KEY,
    KAD_RPC_CONT_DICT_VAL,
    KAD_RPC_CONT_DICT_END,
    KAD_RPC_CONT_LIST_START,
    KAD_RPC_CONT_LIST_ELT,
    KAD_RPC_CONT_LIST_END ,
};

struct kad_rpc_parser {
    const char         *beg;    /* pointer to begin of buffer */
    const char         *cur;    /* pointer to current char in buffer */
    const char         *end;    /* pointer to end of buffer */
    bool                err;
    char                err_msg[KAD_RPC_PARSER_ERR_MSG_MAX];
    enum kad_rpc_mark   stack[KAD_RPC_PARSER_STACK_MAX];
    size_t              stack_off;
    struct kad_rpc_msg  msg;
};

enum kad_rpc_val_type {
    KAD_RPC_VAL_NONE,
    KAD_RPC_VAL_INT,
    KAD_RPC_VAL_STR,
    KAD_RPC_VAL_LIST,
    KAD_RPC_VAL_DICT,
};

struct kad_rpc_val {
    enum kad_rpc_val_type t;
    union {
        long long         i;
        struct {
            /* TODO: use an iobuf instead ? */
            char          p[KAD_RPC_PARSER_STR_MAX];
            size_t        len;
        } s;
    };
};


/* https://github.com/willemt/heapless-bencode/blob/master/bencode.c */
static bool
kad_rpc_parse_int(struct kad_rpc_parser *p, struct kad_rpc_val *val)
{
    val->t = KAD_RPC_VAL_INT;
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

static bool
kad_rpc_parse_str(struct kad_rpc_parser *p, struct kad_rpc_val *val)
{
    val->t = KAD_RPC_VAL_STR;
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

    if (val->s.len > KAD_RPC_PARSER_STR_MAX) {
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
    (void)parser; // FIXME:
}

static bool
kad_rpc_parser_push(struct kad_rpc_parser *p, enum kad_rpc_mark tok)
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
        p->stack[p->stack_off] = KAD_RPC_MARK_NONE;
    }
    return true;
}

/**
 * "t" transaction id: 2 chars.
 * "y" message type: "q" for query, "r" for response, or "e" for error.
 *
 * "q" query method name: str.
 * "a" named arguments: dict.
 * "r" named return values: dict.
 *
 * "e" list: error code (int), error msg (str).
 */
static bool kad_rpc_msg_put(struct kad_rpc_msg *msg,
                            const struct kad_rpc_val *val,
                            const enum kad_rpc_cont emit)
{
    (void)msg; // FIXME:

    switch (emit) {
    case KAD_RPC_CONT_DICT_KEY: {
        log_debug("dict_key: %.*s", val->s.len, val->s.p);
        break;
    }

    case KAD_RPC_CONT_DICT_VAL: {
        if (val->t == KAD_RPC_VAL_INT)
            log_debug("dict_val int: %lld", val->i);
        else if (val->t == KAD_RPC_VAL_STR)
            log_debug("dict_val str: %.*s", val->s.len, val->s.p);
        else
            log_debug("dict_val unsupported: %d", val->t);
        break;
    }

    case KAD_RPC_CONT_LIST_ELT: {
        if (val->t == KAD_RPC_VAL_INT)
            log_debug("list_elt int: %lld", val->i);
        else if (val->t == KAD_RPC_VAL_STR)
            log_debug("list_elt str: %.*s", val->s.len, val->s.p);
        else
            log_debug("list_elt unsupported: %d", val->t);
        break;
    }

    default:
        break;
    }

    return true;
}

/*
  DICT: 'd' STR VAL 'e'
  LIST: 'l' VAL* 'e'
  VAL: STR | INT | DICT | LIST
 */
static bool
kad_rpc_parser_syntax_check(struct kad_rpc_parser *p, struct kad_rpc_val *val,
                            const enum kad_rpc_mark got,
                            enum kad_rpc_cont *emit)
{
    enum kad_rpc_mark mark = got;
    while (mark != KAD_RPC_MARK_NONE && p->stack_off > 0) {
        enum kad_rpc_mark stack_top = p->stack[p->stack_off-1];
        /* log_debug("mark=%u, stack_top=%u, stack_off=%zu", */
        /*           mark, stack_top, p->stack_off); */

        if (mark == KAD_RPC_MARK_VAL) {
            if (stack_top == KAD_RPC_MARK_LIST) {
                *emit = KAD_RPC_CONT_LIST_ELT;
            }
            else if (stack_top == KAD_RPC_MARK_DICT_VAL) {
                kad_rpc_parser_pop(p, 1);
                *emit = KAD_RPC_CONT_DICT_VAL;
                kad_rpc_parser_push(p, KAD_RPC_MARK_DICT_KEY);
            }
            else
                goto fail;
            mark = KAD_RPC_MARK_NONE;
        }

        else if (mark == KAD_RPC_MARK_INT) {
            mark = KAD_RPC_MARK_VAL;
        }

        else if (mark == KAD_RPC_MARK_STR) {
            if (stack_top == KAD_RPC_MARK_DICT_KEY) {
                kad_rpc_parser_pop(p, 1);
                *emit = KAD_RPC_CONT_DICT_KEY;
                kad_rpc_parser_push(p, KAD_RPC_MARK_DICT_VAL);
                mark = KAD_RPC_MARK_NONE;
            }
            else {
                mark = KAD_RPC_MARK_VAL;
            }
        }

        else if (mark == KAD_RPC_MARK_END) {
            if (stack_top == KAD_RPC_MARK_LIST) {
                kad_rpc_parser_pop(p, 1);
                val->t = KAD_RPC_VAL_LIST;
                *emit = KAD_RPC_CONT_LIST_END;
                mark = KAD_RPC_MARK_VAL;
            }
            else if (stack_top == KAD_RPC_MARK_DICT_KEY ||
                     stack_top == KAD_RPC_MARK_DICT_VAL) {
                kad_rpc_parser_pop(p, 1);
                val->t = KAD_RPC_VAL_LIST;
                *emit = KAD_RPC_CONT_DICT_END;
                mark = KAD_RPC_MARK_VAL;
            }
            else
                goto fail;
        }

        else
            goto fail;

        if (mark != KAD_RPC_MARK_NONE && *emit)
            log_debug("__FIXME: emit (%u) will be overridden.", *emit);
    }

    return true;

  fail:
    p->err = true;
    strcpy(p->err_msg, "Syntax error.");
    return false;
}

/* Bottom-up parsing: try to pull tokens one after one another, and possibly
   create nested in collections. This happens in 3 stages: low-level lexer,
   syntax checking, and filling a kad message struct. */
bool kad_rpc_parse(const char host[], const char service[],
                   const char buf[], const size_t slen)
{
    (void)host; (void)service; // FIXME
    bool ret = true;

    struct kad_rpc_parser parser;
    kad_rpc_parser_init(&parser, buf, slen);

    struct kad_rpc_msg msg = {0};
    enum kad_rpc_mark mark = KAD_RPC_MARK_NONE;
    enum kad_rpc_cont emit = KAD_RPC_CONT_NONE;
    struct kad_rpc_val val;
    while (parser.cur != parser.end) {
        memset(&val, 0, sizeof(val));
        mark = KAD_RPC_MARK_NONE;
        emit = KAD_RPC_CONT_NONE;

        if (*parser.cur == 'i') { // int
            if (kad_rpc_parse_int(&parser, &val)) {
                mark = KAD_RPC_MARK_INT;
            }
        }

        else if (isdigit(*parser.cur)) { // string
            if (kad_rpc_parse_str(&parser, &val)) {
                mark = KAD_RPC_MARK_STR;
            }
        }

        else if (*parser.cur == 'l') { // list
            parser.cur++;
            if (kad_rpc_parser_push(&parser, KAD_RPC_MARK_LIST)) {
                emit = KAD_RPC_CONT_LIST_START;
                log_debug("LIST");
            }
        }

        else if (*parser.cur == 'd') { // dict
            parser.cur++;
            /* For dict we store the awaited state, starting with dict_key and
               alternating btw. dict_key/dict_val. */
            if (kad_rpc_parser_push(&parser, KAD_RPC_MARK_DICT_KEY)) {
                emit = KAD_RPC_CONT_DICT_START;
                log_debug("DICT");
            }
        }

        else if (*parser.cur == 'e') {
            parser.cur++;
            mark = KAD_RPC_MARK_END;
            log_debug("END");
        }

        else {
            parser.err = true;
            strcpy(parser.err_msg, "Syntax error."); // TODO: send reply
        }

        if (parser.err ||
            !kad_rpc_parser_syntax_check(&parser, &val, mark, &emit) ||
            !kad_rpc_msg_put(&msg, &val, emit)) {
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
