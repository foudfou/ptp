#ifndef PROTO_H
#define PROTO_H

#include <stdbool.h>
#include <stdlib.h>

enum tlv_stage {
    TLV_STAGE_NONE,
    TLV_STAGE_ERROR,
    TLV_STAGE_TYPE,
    TLV_STAGE_LEN,
    TLV_STAGE_DATA,
};

enum tlv_type {
    TLV_TYPE_NONE,
    TLV_TYPE_ERROR,
    TLV_TYPE_NAME,
    TLV_TYPE_QUERY,
};

typedef struct {
    enum tlv_type  id;
    const char    *name;
} tlv_type_name;

static const tlv_type_name tlv_type_names[] = {
    { TLV_TYPE_ERROR,  "ERRO" },
    { TLV_TYPE_NAME,   "NAME" },
    { TLV_TYPE_QUERY,  "QERY" },
    { 0, NULL }
};

/* TODO: this is probably the skeleton of a de-/serialization framework. See
   http://stackoverflow.com/a/6002598/421846 */
struct proto_iobuf {
    char     *buf;
    size_t    pos;
    unsigned  nchunks;
};

struct proto_parser {
    bool               recv;
    bool               send;
    enum tlv_stage     stage;
    enum tlv_type      msg_type;
    uint32_t           msg_len;
    struct proto_iobuf msg_data;     /* holds only the data field */
};

const char *tlv_type_get_name(const enum tlv_type typ);

void proto_parser_init(struct proto_parser *parser);
void proto_parser_terminate(struct proto_parser *parser);
bool proto_parse(struct proto_parser *parser, const char buf[], const size_t len);

#endif /* PROTO_H */
