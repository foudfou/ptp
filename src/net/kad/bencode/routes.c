/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#include "log.h"
#include "net/kad/bencode/parser.h"
#include "net/kad/bencode/serde.h"
#include "utils/array.h"
#include "net/kad/bencode/routes.h"

enum kad_routes_encoded_key {
    KAD_ROUTES_ENCODED_KEY_NONE,
    KAD_ROUTES_ENCODED_KEY_NODE_ID,
    KAD_ROUTES_ENCODED_KEY_NODES,
};

static const lookup_entry kad_routes_encoded_key_names[] = {
    { KAD_ROUTES_ENCODED_KEY_NODE_ID, "id" },
    { KAD_ROUTES_ENCODED_KEY_NODES,   "nodes" },
    { 0,                             NULL },
};

/**
 * Parses @buf and populates @routes accordingly
 *
 * Our routes object is in the form:
 *
 * {"node-id":"kad_guid_in_raw_bytes_network_order",
 * "nodes":["compact_node_info1", "compact_node_info2", ...]}
 *
 * Where "Compact node info" is a 26-byte string (20-byte node-id + 6-byte
 * "Compact IP-address/port info" (4-byte IP (16-byte for ip6) + 2-byte port
 * all in network byte order))".
 */
bool benc_decode_routes(struct kad_routes_encoded *routes, const char buf[], const size_t slen) {
    benc_repr_init();

    if (!benc_parse(&repr, buf, slen)) {
        return false;
    }

    if (repr.n[0].typ != BENC_NODE_TYPE_DICT) {
        log_error("Decoded bencode object not a dict.");
        return false;
    }

    const char *key = lookup_by_id(kad_routes_encoded_key_names, KAD_ROUTES_ENCODED_KEY_NODE_ID);
    const struct benc_node *n = benc_node_find_literal_str(&repr.n[0], key, strlen(key));
    if (!n || n->chd[0]->lit->s.len != KAD_GUID_SPACE_IN_BYTES) {
        return false;
    }
    if (!benc_read_guid(&routes->self_id, n->chd[0]->lit)) {
        log_error("Node_id copy failed.");
        return false;
    }
    int nnodes = benc_read_nodes_from_key(routes->nodes, ARRAY_LEN(routes->nodes), &repr.n[0],
                                          kad_routes_encoded_key_names,
                                          KAD_ROUTES_ENCODED_KEY_NODES, KAD_ROUTES_ENCODED_KEY_NONE);
    if (nnodes < 0) {
        return false;
    }
    routes->nodes_len = nnodes;

    return true;
}

/**
 * Serialize a representation of routes.
 *
 * The intermediary representation is not stricly necessary, as we could use a
 * struct kad_routes as input. It's just more consistent with the deserialization
 * operation.
 */
bool benc_encode_routes(struct iobuf *buf, const struct kad_routes_encoded *routes)
{
    char tmps[2048];
    size_t tmps_len = 0;

    iobuf_append(buf, "d", 1);

    // id
    const char *field_name = lookup_by_id(kad_routes_encoded_key_names, KAD_ROUTES_ENCODED_KEY_NODE_ID);
    sprintf(tmps, "%zu:%s", strlen(field_name), field_name);
    iobuf_append(buf, tmps, strlen(tmps));
    sprintf(tmps, "%d:", KAD_GUID_SPACE_IN_BYTES);
    tmps_len = strlen(tmps);
    memcpy(tmps + tmps_len, (char*)routes->self_id.bytes, KAD_GUID_SPACE_IN_BYTES);
    tmps_len += KAD_GUID_SPACE_IN_BYTES;
    iobuf_append(buf, tmps, tmps_len);

    // nodes
    field_name = lookup_by_id(kad_routes_encoded_key_names, KAD_ROUTES_ENCODED_KEY_NODES);
    sprintf(tmps, "%zu:%s", strlen(field_name), field_name);
    iobuf_append(buf, tmps, strlen(tmps));
    iobuf_append(buf, "l", 1);
    if (!benc_write_nodes(buf, routes->nodes, routes->nodes_len)) {
        return false;
    }
    iobuf_append(buf, "ee", 2);

    return true;
}

int benc_decode_bootstrap_nodes(struct kad_node_info nodes[],
                                const size_t nodes_len,
                                const char buf[], const size_t slen)
{
    benc_repr_init();

    if (!benc_parse(&repr, buf, slen)) {
        return -1;
    }

    const struct benc_node *n = &repr.n[0];
    if (n->typ != BENC_NODE_TYPE_LIST) {
        log_error("Object is not a list.");
        return -1;
    }

    int nnodes = benc_read_nodes(nodes, nodes_len, n);
    if (nnodes < 0) {
        log_error("Reading bencoded nodes addresses failed.");
        return -1;
    }

    return nnodes;
}
