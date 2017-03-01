/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#ifndef PROTO_MSG_H
#define PROTO_MSG_H

/**
 * Peers exchange messages in the Type-Length-Data (tlv) format.
 * The Type field contains the command or the message type.
 * The Length field contains the data length in uint32_t (~4GB).
 * The Data field contains raw bytes.
 *
 * Inspired from http://cs.berry.edu/~nhamid/p2p/framework-python.html
 */

#include <stdbool.h>
#include <stdlib.h>
#include "net/iobuf.h"
#include "net/serialization.h"

#define PROTO_MSG_FIELD_TYPE_LEN    4
#define PROTO_MSG_FIELD_LENGTH_LEN  4

enum proto_msg_stage {
    PROTO_MSG_STAGE_NONE,
    PROTO_MSG_STAGE_ERROR,
    PROTO_MSG_STAGE_TYPE,
    PROTO_MSG_STAGE_LEN,
    PROTO_MSG_STAGE_DATA,
};

enum proto_msg_type {
    PROTO_MSG_TYPE_NONE,
    PROTO_MSG_TYPE_ERROR,
    PROTO_MSG_TYPE_NAME,
    PROTO_MSG_TYPE_QUERY,
};

typedef struct {
    enum proto_msg_type  id;
    const char          *name;
} proto_msg_type_name;

static const proto_msg_type_name proto_msg_type_names[] = {
    { PROTO_MSG_TYPE_ERROR,  "ERRO" },
    { PROTO_MSG_TYPE_NAME,   "NAME" },
    { PROTO_MSG_TYPE_QUERY,  "QERY" },
    { 0, NULL }
};

struct proto_msg_parser {
    bool                 recv;
    bool                 send;
    enum proto_msg_stage stage;
    enum proto_msg_type  msg_type;
    union u32            msg_len;
    struct iobuf         msg_data; /* holds only the data field */
};

const char *proto_msg_type_get_name(const enum proto_msg_type typ);

void proto_msg_parser_init(struct proto_msg_parser *parser);
void proto_msg_parser_terminate(struct proto_msg_parser *parser);
bool proto_msg_parse(struct proto_msg_parser *parser, const char buf[], const size_t len);

#endif /* PROTO_MSG_H */
