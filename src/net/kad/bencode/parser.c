/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "net/kad/bencode/parser.h"

#define POINTER_OFFSET(beg, end) ((ptrdiff_t)((end) - (beg)))


/* https://github.com/willemt/heapless-bencode/blob/master/bencode.c */
static bool benc_extract_int(struct benc_parser *p, struct benc_literal *lit)
{
    lit->t = BENC_LITERAL_TYPE_INT;
    long long val_tmp = lit->i = 0;
    int sign = 1;

    p->cur++;  // eat up 'i'
    if (*p->cur == '-') {
        sign = -1;
        p->cur++;
    }

    do {
        if (!isdigit(*p->cur)) {
            sprintf(p->err_msg, "Invalid character in bencode at %zu.",
                    POINTER_OFFSET(p->beg, p->cur));
            p->err = true;
            return false;
        }

        val_tmp *= 10;
        long long val_digit = *p->cur - '0';
        if (val_tmp > LLONG_MAX - val_digit) {
            sprintf(p->err_msg, "Overflow in int parsing at %zu.",
                    POINTER_OFFSET(p->beg, p->cur));
            p->err = true;
            return false;
        }
        else {
            val_tmp += val_digit;
        }

        lit->i = val_tmp;
        p->cur++;
    } while (*p->cur != 'e');
    p->cur++;  // eat up 'e'

    lit->i *= sign;

    return true;
}

static bool benc_extract_str(struct benc_parser *p, struct benc_literal *lit)
{
    lit->t = BENC_LITERAL_TYPE_STR;
    lit->s.len = 0;
    do {
        if (!isdigit(*p->cur)) {
            sprintf(p->err_msg, "Invalid character in bencode at %zu.",
                    POINTER_OFFSET(p->beg, p->cur));
            p->err = true;
            return false;
        }

        lit->s.len *= 10;
        lit->s.len += *p->cur - '0';
        p->cur++;
    } while (*p->cur != ':');

    if (lit->s.len > BENC_PARSER_STR_LEN_MAX) {
        sprintf(p->err_msg, "String too long at %zu.",
                POINTER_OFFSET(p->beg, p->cur));
        p->err = true;
        return false;
    }

    p->cur++;
    memcpy(lit->s.p, p->cur, lit->s.len);
    p->cur += lit->s.len;

    return true;
}

static void benc_parser_init(struct benc_parser *parser,
                             const char *buf, const size_t slen)
{
    parser->err = false;
    parser->cur = parser->beg = buf;
    parser->end = buf + slen;
}

static void benc_parser_terminate(struct benc_parser *parser)
{
    (void)parser; // FIXME:
}

static bool benc_stack_push(struct benc_parser *p, const size_t node_idx)
{
    if (p->stack_off >= BENC_PARSER_STACK_MAX - 1) {
        p->err = true;
        strcpy(p->err_msg, "Parser stack reached maximum nested level.");
        return false;
    }
    p->stack[p->stack_off] = node_idx;
    p->stack_off++;
    return true;
}

static bool benc_stack_pop(struct benc_parser *p)
{
    if (p->stack_off == 0) {
        p->err = true;
        strcpy(p->err_msg, "Attempt to pop empty parser stack.");
        return false;
    }
    p->stack_off--;
    // No need to clear index, just decrement stack_off
    return true;
}

static size_t
benc_repr_add_node(struct benc_repr *repr,
                   const enum benc_node_type typ,
                   const struct benc_literal *lit)
{
    struct benc_node n = {0};
    struct benc_literal *litp = NULL;

    if (typ == BENC_NODE_TYPE_LITERAL) {
        if (!benc_literal_lst_append(&repr->lit, lit, 1)) {
            log_error("repr literal append failed");
            return INVALID_INDEX;
        }
        size_t lit_idx = repr->lit.len - 1;
        litp = &repr->lit.buf[lit_idx];
        if (lit->t == BENC_LITERAL_TYPE_STR) {
            memcpy(litp->s.p, lit->s.p, lit->s.len);
        }
        n.typ = typ;
        n.lit = lit_idx;
    }

    else if (typ == BENC_NODE_TYPE_DICT_ENTRY) {
        n.typ = typ;
        memcpy(n.k, lit->s.p, lit->s.len);
        n.k_len = lit->s.len;
    }

    else if (typ == BENC_NODE_TYPE_LIST ||
             typ == BENC_NODE_TYPE_DICT) {
        n.typ = typ;
    }

    else {
        return INVALID_INDEX;
    }

    size_t node_idx = repr->n.len;
    if (!benc_node_lst_append(&repr->n, &n, 1)) {
        log_error("repr node append failed");
        return INVALID_INDEX;
    }

    log_debug("node typ=%d added at index %zu", n.typ, node_idx);
    return node_idx;
}


static bool
benc_repr_attach_node(struct benc_node *parent, const size_t node_idx)
{
    return index_lst_append(&parent->chd, &node_idx, 1);
}

/*
void log_debug_literal(const struct benc_literal *lit)
{
    if (lit->t == BENC_LITERAL_TYPE_INT)
        log_debug("literal int: %lld", lit->i);
    else if (lit->t == BENC_LITERAL_TYPE_STR)
        log_debug("literal str: %.*s", lit->s.len, lit->s.p);
    else
        log_debug("literal unsupported: %d", lit->t);
}
*/

struct benc_node*
benc_node_find_key(const struct benc_repr *repr,
                   const struct benc_node *dict,
                   const char key[], const size_t key_len)
{
    if (dict->typ != BENC_NODE_TYPE_DICT) {
        return NULL;
    }

    struct benc_node *entry = NULL;
    for (size_t i = 0; i < dict->chd.len; ++i) {
        size_t node_idx = dict->chd.buf[i];
        struct benc_node *n = &repr->n.buf[node_idx];
        if (n->typ == BENC_NODE_TYPE_DICT_ENTRY &&
            n->k_len == key_len &&
            strncmp(n->k, key, key_len) == 0) {
            entry = n;
            break;
        }
    }
    return entry;
}

static bool
benc_repr_build(struct benc_repr *repr, struct benc_parser *p,
                const struct benc_literal *lit, const enum benc_tok tok)
{
    size_t node_idx = INVALID_INDEX;
    size_t stack_top_idx = INVALID_INDEX;
    struct benc_node *stack_top = NULL;

    if (p->stack_off > 0) {
        stack_top_idx = p->stack[p->stack_off - 1];
    }

    /* We only allow a single object, no juxtaposition. There is a single
       entry point in a benc_repr: the root node, repr->n.buf[0]. */
    if (stack_top_idx == INVALID_INDEX && repr->n.len > 0 && tok != BENC_TOK_END) {
        log_error("Orphan node not allowed");
        return false;
    }

    switch (tok) {
    case BENC_TOK_LITERAL:
        /* log_debug("  lit->t=%d stack_top=%zu stack_top->typ=%d", lit->t, stack_top_idx, repr->n.buf[stack_top_idx].typ); */
        // dict key
        if (lit->t == BENC_LITERAL_TYPE_STR &&
            stack_top_idx != INVALID_INDEX &&
            repr->n.buf[stack_top_idx].typ == BENC_NODE_TYPE_DICT)
        {
            const struct benc_node *dup =
                benc_node_find_key(repr, &repr->n.buf[stack_top_idx], lit->s.p, lit->s.len);
            if (dup) {
                log_error("Duplicate dict_entry");
                return false;
            };

            node_idx = benc_repr_add_node(repr, BENC_NODE_TYPE_DICT_ENTRY, lit);
            if (node_idx == INVALID_INDEX) {
                log_error("Can't add dict_entry node");
                return false;
            }

            stack_top = &repr->n.buf[stack_top_idx]; // fresh pointer (potential realloc in benc_repr_add_node)
            if (!benc_repr_attach_node(stack_top, node_idx)) {
                log_error("Can't attach dict_entry node to dict");
                return false;
            }

            if (!benc_stack_push(p, node_idx)) {
                log_error("Can't stack_push dict_entry node");
                return false;
            }
        }

        // normal case
        else {
            node_idx = benc_repr_add_node(repr, BENC_NODE_TYPE_LITERAL, lit);
            if (node_idx == INVALID_INDEX) {
                log_error("Can't add literal node");
                return false;
            }

            if (stack_top_idx != INVALID_INDEX) {
                stack_top = &repr->n.buf[stack_top_idx]; // fresh pointer
                if (stack_top->typ == BENC_NODE_TYPE_DICT_ENTRY) {
                    if (!benc_repr_attach_node(stack_top, node_idx) ||
                        !benc_stack_pop(p)) {
                        log_error("Can't attach literal node to dict_entry or stack_pop");
                        return false;
                    }
                }
                else if (stack_top->typ == BENC_NODE_TYPE_LIST) {
                    if (!benc_repr_attach_node(stack_top, node_idx)) {
                        log_error("Can't attach literal node to list");
                        return false;
                    }
                }
                else {
                    // nop
                }
            }
        }

        break;

    case BENC_TOK_LIST:
    case BENC_TOK_DICT: {
        enum benc_node_type node_type = BENC_NODE_TYPE_NONE;
        if (tok == BENC_TOK_LIST) {
            node_type = BENC_NODE_TYPE_LIST;
        } else if (tok == BENC_TOK_DICT) {
            node_type = BENC_NODE_TYPE_DICT;
        }
        node_idx = benc_repr_add_node(repr, node_type, lit);
        if (node_idx == INVALID_INDEX) {
            log_error("Can't add list/dict node");
            return false;
        }

        if (stack_top_idx != INVALID_INDEX) {
            stack_top = &repr->n.buf[stack_top_idx]; // fresh pointer
            if (stack_top->typ == BENC_NODE_TYPE_DICT_ENTRY) {
                if (!benc_repr_attach_node(stack_top, node_idx) ||
                    !benc_stack_pop(p)) {
                    log_error("Can't attach list/dict node to dict_entry or stack_pop");
                    return false;
                }
            }
            else if (stack_top->typ == BENC_NODE_TYPE_LIST) {
                if (!benc_repr_attach_node(stack_top, node_idx)) {
                    log_error("Can't attach list/dict node to list");
                    return false;
                }
            }
            else {
                // nop
            }
        }

        if (!benc_stack_push(p, node_idx)) return false;

        break;
    }

    case BENC_TOK_END: {
        if (!benc_stack_pop(p)) {
            return false;
        }

        break;
    }

    default:
        goto fail;
    }

    return true;

  fail:
    p->err = true;
    strcpy(p->err_msg, "Syntax error.");
    return false;
}

/*
 * Bottom-up stream parsing: try to pull tokens one after one another, possibly
 * creating nested collections. This happens in a 2-stage loop: low-level
 * lexer, then bencode representation building.
 */
bool benc_parse(struct benc_repr *repr, const char buf[], const size_t slen)
{
    if (!slen) {
        log_error("Invalid void message.");
        return false;
    }

    bool ret = true;

    struct benc_parser parser = {0};
    benc_parser_init(&parser, buf, slen);

    struct benc_literal lit;
    enum benc_tok tok = BENC_TOK_NONE;
    while (parser.cur != parser.end) {
        memset(&lit, 0, sizeof(lit));
        tok = BENC_TOK_NONE;

        if (*parser.cur == 'i') { // int
            if (benc_extract_int(&parser, &lit)) {
                tok = BENC_TOK_LITERAL;
                log_debug("INT");
            }
            else {log_error("Extract int failed");}
        }

        else if (isdigit(*parser.cur)) { // string
            if (benc_extract_str(&parser, &lit)) {
                tok = BENC_TOK_LITERAL;
                log_debug("STR");
            }
            else {log_error("Extract str failed");}
        }

        else if (*parser.cur == 'l') { // list
            parser.cur++;
            tok = BENC_TOK_LIST;
            log_debug("LIST");
        }

        else if (*parser.cur == 'd') { // dict
            parser.cur++;
            tok = BENC_TOK_DICT;
            log_debug("DICT");
        }

        else if (*parser.cur == 'e') {
            parser.cur++;
            tok = BENC_TOK_END;
            log_debug("END");
        }

        else {
            parser.err = true;
            strcpy(parser.err_msg, "Syntax error."); // TODO: send reply
        }

        /* log_debug("  tok=%d parser.err=%d", tok, parser.err); */
        if (tok == BENC_TOK_NONE ||
            parser.err ||
            !benc_repr_build(repr, &parser, &lit, tok)) {
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
