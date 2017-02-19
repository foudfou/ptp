#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "log.h"
#include "utils/safe.h"
#include "proto.h"

#define PROTO_IO_CHUNK 512

/**
 * Peers exchange messages in the Type-Length-Data (tlv) format.
 * The Type field contains the command or the message type.
 * The Length field contains the data length in uint32_t (~4GB).
 * The Data field contains raw bytes.
 *
 * Inspired from http://cs.berry.edu/~nhamid/p2p/framework-python.html
 */

static bool proto_iobuf_grow(struct proto_iobuf *buff)
{
    size_t nchk = buff->nchunks + 1;
    buff->buf = realloc(buff->buf, nchk * PROTO_IO_CHUNK);
    if (!buff->buf) {
        log_perror("Failed realloc: %s.", errno);
        return false;
    }
    buff->nchunks = nchk;
    return true;
}

static bool proto_iobuf_shrink(struct proto_iobuf *buff)
{
    return true;
}

void proto_parser_init(struct proto_parser *parser)
{
    memset(parser, 0, sizeof(struct proto_parser));
    /* The following are not required but explicit is better. */
    parser->recv     = false;
    parser->send     = false;
    parser->stage    = TLV_STAGE_NONE;
    parser->msg_type = TLV_TYPE_NONE;
}

void proto_parser_terminate(struct proto_parser *parser)
{
    safe_free(parser->msg_data.buf);
}

const char *tlv_type_get_name(const enum tlv_type typ)
{
    const tlv_type_name *tok = tlv_type_names;
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

static bool tlv_type_parse(const char buf[], enum tlv_type *otyp)
{
    const tlv_type_name *tok = tlv_type_names;
    while (tok->name) {
        if (strncmp(tok->name, buf, 4) == 0) break;
        tok++;
    }
    if (!tok->name) {
        log_error("Message type not found.");
        return false;
    }
    *otyp = tok->id;
    return true;
}

static bool tlv_len_parse(const char buf[], size_t pos, uint32_t *len)
{
    uint32_t raw32;
    memcpy(&raw32, buf + pos, 4);
    *len = ntohl(raw32);
    return true;
}

bool proto_parse(struct proto_parser *parser, const char buf[], const size_t len)
{
    parser->recv = true;
    size_t offset = 0;

    while (offset < len) {
        switch (parser->stage) {
        case TLV_STAGE_NONE: {
            parser->stage = TLV_STAGE_TYPE;
            break;
        }

        case TLV_STAGE_ERROR: {
            log_debug_hex(buf, len);
            return false;
            break;
        }

        case TLV_STAGE_TYPE: {
            if (len < 4) {
                log_error("Message too short.");
                parser->stage = TLV_STAGE_ERROR;
                break;
            }

            if (!tlv_type_parse(buf, &parser->msg_type)) {
                log_warning("Ignoring further input.");
                parser->stage = TLV_STAGE_ERROR;
                break;
            }

            log_debug("  msg_type=%u", parser->msg_type);
            offset += 4;
            parser->stage = TLV_STAGE_LEN;
            break;
        }

        case TLV_STAGE_LEN: {
            if (len - offset < 4) {
                log_error("Message too short.");
                parser->stage = TLV_STAGE_ERROR;
                break;
            }

            parser->msg_len = 0;
            if (!tlv_len_parse(buf, offset, &parser->msg_len)) {
                log_warning("Ignoring further input.");
                parser->stage = TLV_STAGE_ERROR;
                break;
            }


            log_debug("  msg_len=%u", parser->msg_len);
            offset += 4;
            parser->stage = TLV_STAGE_DATA;
            break;
        }

        case TLV_STAGE_DATA: {
            size_t data_len = len - offset;

            log_debug("  msg_data_pos=%zu, data_len=%zu, msg_data size=%zu",
                      parser->msg_data.pos, data_len,
                      parser->msg_data.nchunks * PROTO_IO_CHUNK);
            if ((parser->msg_data.pos + data_len >
                 parser->msg_data.nchunks * PROTO_IO_CHUNK)
                && !proto_iobuf_grow(&parser->msg_data)) {
                proto_parser_terminate(parser);
                return false;
            }
            log_debug("  msg_data_pos=%zu, data_len=%zu, msg_data size=%zu",
                      parser->msg_data.pos, data_len,
                      parser->msg_data.nchunks * PROTO_IO_CHUNK);

            memcpy(parser->msg_data.buf + parser->msg_data.pos, buf + offset,
                   data_len);
            parser->msg_data.pos += data_len;
            log_debug_hex(parser->msg_data.buf, parser->msg_data.pos);

            /* We check for the length of actually received data after having
               copied it for fear of losing some. */
            if (parser->msg_data.pos > parser->msg_len) {
                log_warning("Received more data than expected.");
                parser->stage = TLV_STAGE_ERROR;
            }
            if (parser->msg_data.pos == parser->msg_len)
                parser->stage = TLV_STAGE_NONE;

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
