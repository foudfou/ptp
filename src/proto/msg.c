/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "log.h"
#include "proto/msg.h"

void proto_msg_parser_init(struct proto_msg_parser *parser)
{
    memset(parser, 0, sizeof(struct proto_msg_parser));
    /* The following are not required but explicit is better. */
    parser->recv     = false;
    parser->send     = false;
    parser->stage    = PROTO_MSG_STAGE_NONE;
    parser->msg_type = PROTO_MSG_TYPE_NONE;
}

void proto_msg_parser_terminate(struct proto_msg_parser *parser)
{
    iobuf_reset(&parser->msg_data);
}

const char *proto_msg_type_get_name(const enum proto_msg_type typ)
{
    const proto_msg_type_name *tok = proto_msg_type_names;
    while (tok->name) {
        if (tok->id == typ) break;
        tok++;
    }
    if (!tok->name) {
        log_error("Unknow message type.");
        return NULL;
    }
    return tok->name;
}

static bool proto_msg_type_parse(const char buf[], enum proto_msg_type *otyp)
{
    const proto_msg_type_name *tok = proto_msg_type_names;
    while (tok->name) {
        if (strncmp(tok->name, buf, PROTO_MSG_FIELD_TYPE_LEN) == 0) break;
        tok++;
    }
    if (!tok->name) {
        log_error("Message type not found.");
        return false;
    }
    *otyp = tok->id;
    return true;
}

static void proto_msg_len_parse(const char buf[], size_t pos, union u32 *len)
{
    memcpy(len->db, buf + pos, PROTO_MSG_FIELD_LENGTH_LEN);
    *len = u32_ntoh(*len);
}

bool proto_msg_parse(struct proto_msg_parser *parser, const char buf[], const size_t len)
{
    parser->recv = true;
    size_t offset = 0;

    while (offset < len) {
        switch (parser->stage) {
        case PROTO_MSG_STAGE_NONE: {
            parser->stage = PROTO_MSG_STAGE_TYPE;
            break;
        }

        case PROTO_MSG_STAGE_ERROR: {
            log_debug_hex(buf, len);
            return false;
            break;
        }

        case PROTO_MSG_STAGE_TYPE: {
            if (len < PROTO_MSG_FIELD_TYPE_LEN) {
                log_error("Message too short.");
                parser->stage = PROTO_MSG_STAGE_ERROR;
                break;
            }

            if (!proto_msg_type_parse(buf, &parser->msg_type)) {
                log_warning("Ignoring further input.");
                parser->stage = PROTO_MSG_STAGE_ERROR;
                break;
            }

            log_debug("  msg_type=%u", parser->msg_type);
            offset += PROTO_MSG_FIELD_TYPE_LEN;
            parser->stage = PROTO_MSG_STAGE_LEN;
            break;
        }

        case PROTO_MSG_STAGE_LEN: {
            if (len - offset < PROTO_MSG_FIELD_LENGTH_LEN) {
                log_error("Message too short.");
                parser->stage = PROTO_MSG_STAGE_ERROR;
                break;
            }

            parser->msg_len.dd = 0;
            proto_msg_len_parse(buf, offset, &parser->msg_len);
            log_debug("  msg_len=%"PRIu32, parser->msg_len.dd);
            offset += PROTO_MSG_FIELD_LENGTH_LEN;
            parser->stage = PROTO_MSG_STAGE_DATA;
            break;
        }

        case PROTO_MSG_STAGE_DATA: {
            if (!iobuf_append(&parser->msg_data, buf + offset, len - offset)) {
                proto_msg_parser_terminate(parser);
                return false;
            }

            /* We check for the length of actually received data after having
               copied it for fear of losing some. */
            if (parser->msg_data.pos > parser->msg_len.dd) {
                log_warning("Received more data than expected.");
                parser->stage = PROTO_MSG_STAGE_ERROR;
            }
            if (parser->msg_data.pos == parser->msg_len.dd)
                parser->stage = PROTO_MSG_STAGE_NONE;

            goto while_end; // we've got all we need from this chunk
            break;
        }

        default:
            log_error("Parser in unknown state.");
            return false;
            break;
        }
    } while_end:

    return true;
}
