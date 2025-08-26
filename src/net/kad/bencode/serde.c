/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#include "log.h"
#include "net/socket.h"
#include "net/kad/bencode/parser.h"
#include "net/kad/bencode/serde.h"

const struct benc_node*
benc_node_find_literal_str(const struct benc_repr *repr,
                           const struct benc_node *dict,
                           const char key[], const size_t key_len)
{
    struct benc_node *n = benc_node_find_key(repr, dict, key, key_len);
    if (!n) {
        log_error("Missing entry (%s) in decoded bencode object.", key);
        return NULL;
    }
    struct benc_node *child = benc_node_get_first_child(repr, n);
    if (!child || child->typ != BENC_NODE_TYPE_LITERAL) {
        log_error("Invalid entry %s.", key);
        return NULL;
    }
    struct benc_literal *lit = benc_node_get_literal(repr, child);
    if (!lit || lit->t != BENC_LITERAL_TYPE_STR) {
        log_error("Invalid entry %s.", key);
        return NULL;
    }
    return n;
}

bool benc_read_guid(kad_guid *id, const struct benc_literal *lit)
{
    if (lit->t != BENC_LITERAL_TYPE_STR) {
        log_error("Message node id not a string.");
        return false;
    }
    if (lit->s.len != KAD_GUID_SPACE_IN_BYTES) {
        log_error("Message node id has wrong length (%zu).", lit->s.len);
        return false;
    }
    kad_guid_set(id, (unsigned char*)lit->s.p);
    return true;
}

const struct benc_node*
benc_node_navigate_to_key(const struct benc_repr *repr,
                          const struct benc_node *dict,
                          const lookup_entry k_names[],
                          const int k1, const int k2)
{
    const char *key = lookup_by_id(k_names, k1);
    struct benc_node *n = benc_node_find_key(repr, dict, key, strlen(key));
    if (!n) {
        log_warning("Missing entry (%s) in decoded bencode object.", key);
        return NULL;
    }
    if (k2 == 0) {  // generic *_KEY_NONE
        return n;
    }

    struct benc_node *child = benc_node_get_first_child(repr, n);
    if (!child || child->typ != BENC_NODE_TYPE_DICT) {
        log_error("Invalid entry %s.", key);
        return NULL;
    }
    key = lookup_by_id(k_names, k2);
    n = benc_node_find_key(repr, child, key, strlen(key));
    if (!n) {
        log_warning("Missing entry (%s) in decoded bencode object.", key);
        return NULL;
    }
    return n;
}

static bool benc_read_single_addr(struct sockaddr_storage *addr, char *p, size_t len)
{
    switch (len) {
    case BENC_IP4_ADDR_LEN_IN_BYTES + 2: {
        struct sockaddr_in *sa = (struct sockaddr_in*)addr;
        sa->sin_family = AF_INET;
        memcpy(&sa->sin_addr, (unsigned char*)p, BENC_IP4_ADDR_LEN_IN_BYTES);
        p += BENC_IP4_ADDR_LEN_IN_BYTES;
        memcpy(&sa->sin_port, (unsigned char*)p, 2);
        break;
    }

    case BENC_IP6_ADDR_LEN_IN_BYTES + 2: {
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6*)addr;
        sa6->sin6_family = AF_INET6;
        memcpy(&sa6->sin6_addr, (unsigned char*)p, BENC_IP6_ADDR_LEN_IN_BYTES);
        p += BENC_IP6_ADDR_LEN_IN_BYTES;
        memcpy(&sa6->sin6_port, (unsigned char*)p, 2);
        break;
    }

    default:
        log_error("Failed to read single addr.");
        return false;
    }

    return true;
}

int benc_read_nodes(const struct benc_repr *repr,
                    struct kad_node_info nodes[], const size_t nodes_len,
                    const struct benc_node *list)
{
    int nnodes = list->chd.len;
    if ((size_t)nnodes > nodes_len) {
        log_error("Insufficent array size for read nodes.");
        return -1;
    }

    for (int i = 0; i < nnodes; i++) {
        size_t node_idx = list->chd.buf[i];
        const struct benc_node *node = &repr->n.buf[node_idx];
        if (node->typ != BENC_NODE_TYPE_LITERAL) {
            log_error("Invalid node entry #%d.", i);
            return -1;
        }

        const struct benc_literal *lit = benc_node_get_literal(repr, node);
        if (!lit || lit->t != BENC_LITERAL_TYPE_STR) {
            log_error("Invalid node entry #%d.", i);
            return -1;
        }

        if (lit->s.len < KAD_GUID_SPACE_IN_BYTES ||
            !benc_read_single_addr(&nodes[i].addr,
                                   (char*)(lit->s.p + KAD_GUID_SPACE_IN_BYTES),
                                   lit->s.len - KAD_GUID_SPACE_IN_BYTES)) {
            log_error("Invalid node info in position #%d.", i);
            return -1;
        }
        // only set guid when necessary
        kad_guid_set(&nodes[i].id, (unsigned char*)lit->s.p);

        sockaddr_storage_fmt(nodes[i].addr_str, &nodes[i].addr);
    }

    return nnodes;
}

int benc_read_nodes_from_key(const struct benc_repr *repr,
                             struct kad_node_info nodes[], const size_t nodes_len,
                             const struct benc_node *dict,
                             const lookup_entry k_names[],
                             const int k1, const int k2)
{
    const struct benc_node *n = benc_node_navigate_to_key(repr, dict, k_names, k1, k2);
    if (!n) {
        return -1;
    }

    const char *key = lookup_by_id(k_names, k2 == 0 ? k1 : k2);
    if (benc_node_get_first_child(repr, n)->typ != BENC_NODE_TYPE_LIST) {
        log_error("Invalid entry %s.", key);
        return -1;
    }

    int nnodes = benc_read_nodes(repr, nodes, nodes_len, benc_node_get_first_child(repr, n));
    if (nnodes < 0) {
        log_error("Failed to read nodes from bencode object.");
        return nnodes;
    }

    return nnodes;
}

bool benc_write_nodes(struct iobuf *buf, const struct kad_node_info nodes[], size_t nodes_len)
{
    char tmps[64];
    for (size_t i = 0; i < nodes_len; i++) {
        unsigned compact_len = 0;
        unsigned char compact[21] = {0};
        const struct sockaddr_storage *ss = &nodes[i].addr;
        if (ss->ss_family == AF_INET) {
            compact_len = 6;
            const struct sockaddr_in *sa = (struct sockaddr_in *)ss;
            memcpy(compact, (unsigned char*)&sa->sin_addr, 4);
            memcpy(compact + 4, (unsigned char*)&sa->sin_port, 2);
        }
        else if (ss->ss_family == AF_INET6) {
            compact_len = 18;
            const struct sockaddr_in6 *sa = (struct sockaddr_in6 *)ss;
            memcpy(compact, (unsigned char*)&sa->sin6_addr, 16);
            memcpy(compact + 16, (unsigned char*)&sa->sin6_port, 2);
        }
        else {
            log_error("Unsupported socket address family (%d).", ss->ss_family);
            return false;
        }

        sprintf(tmps, "%u:", KAD_GUID_SPACE_IN_BYTES + compact_len);
        size_t tmps_len = strlen(tmps);
        memcpy(tmps + tmps_len, (char*)nodes[i].id.bytes, KAD_GUID_SPACE_IN_BYTES);
        tmps_len += KAD_GUID_SPACE_IN_BYTES;
        iobuf_append(buf, tmps, tmps_len);
        iobuf_append(buf, (char*)compact, compact_len);
    }

    return true;
}
