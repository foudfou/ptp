/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#include "log.h"
#include "net/kad/bencode/parser.h"
#include "net/kad/bencode/serde.h"

const struct benc_node*
benc_node_find_literal_str(const struct benc_node *dict,
                           const char key[], const size_t key_len)
{
    struct benc_node *n = benc_node_find_key(dict, key, key_len);
    if (!n) {
        log_error("Missing entry (%s) in decoded bencode object.", key);
        return NULL;
    }
    if (n->chd[0]->typ != BENC_NODE_TYPE_LITERAL ||
        n->chd[0]->lit->t != BENC_LITERAL_TYPE_STR) {
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
benc_node_navigate_to_key(const struct benc_node *dict,
                                 const lookup_entry k_names[],
                                 const int k1, const int k2)
{
    const char *key = lookup_by_id(k_names, k1);
    struct benc_node *n = benc_node_find_key(dict, key, strlen(key));
    if (!n) {
        log_warning("Missing entry (%s) in decoded bencode object.", key);
        return NULL;
    }
    if (k2 == 0) {  // generic *_KEY_NONE
        return n;
    }

    if (n->chd[0]->typ != BENC_NODE_TYPE_DICT) {
        log_error("Invalid entry %s.", key);
        return NULL;
    }
    key = lookup_by_id(k_names, k2);
    n = benc_node_find_key(n->chd[0], key, strlen(key));
    if (!n) {
        log_warning("Missing entry (%s) in decoded bencode object.", key);
        return NULL;
    }
    return n;
}

bool benc_read_nodes_from_key(struct kad_node_info nodes[], size_t *nodes_len,
                                 const struct benc_node *dict,
                                 const lookup_entry k_names[],
                                 const int k1, const int k2)
{
    const struct benc_node *n = benc_node_navigate_to_key(dict, k_names, k1, k2);
    if (!n) {
        return false;
    }

    const char *key = lookup_by_id(k_names, k2 == 0 ? k1 : k2);
    if (n->chd[0]->typ != BENC_NODE_TYPE_LIST) {
        log_error("Invalid entry %s.", key);
        return false;
    }

    *nodes_len = n->chd[0]->chd_off;
    for (size_t i = 0; i < *nodes_len; i++) {
        const struct benc_node *node = n->chd[0]->chd[i];
        if (node->typ != BENC_NODE_TYPE_LITERAL ||
            node->lit->t != BENC_LITERAL_TYPE_STR) {
            log_error("Invalid node entry #%d.", i);
            return false;
        }

        if (node->lit->s.len == BENC_KAD_NODE_INFO_IP4_LEN_IN_BYTES) {
            kad_guid_set(&nodes[i].id, (unsigned char*)node->lit->s.p);
            struct sockaddr_in *sa = (struct sockaddr_in*)&nodes[i].addr;
            memcpy(&sa->sin_addr, (unsigned char*)node->lit->s.p + 20, 4);
            memcpy(&sa->sin_port, (unsigned char*)node->lit->s.p + 24, 2);
        }
        else if (node->lit->s.len == BENC_KAD_NODE_INFO_IP6_LEN_IN_BYTES) {
            kad_guid_set(&nodes[i].id, (unsigned char*)node->lit->s.p);
            struct sockaddr_in6 *sa = (struct sockaddr_in6*)&nodes[i].addr;
            memcpy(&sa->sin6_addr, (unsigned char*)node->lit->s.p + 20, 16);
            memcpy(&sa->sin6_port, (unsigned char*)node->lit->s.p + 36, 2);
        }
        else {
            log_error("Invalid node info in position #%d.", i);
            return false;
        }
    }

    return true;
}

bool benc_write_nodes(struct iobuf *buf, const struct kad_node_info nodes[], size_t nodes_len)
{
    char tmps[64];
    size_t tmps_len = 0;

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

        sprintf(tmps, "%d:", KAD_GUID_SPACE_IN_BYTES + compact_len);
        tmps_len = strlen(tmps);
        memcpy(tmps + tmps_len, (char*)nodes[i].id.bytes, KAD_GUID_SPACE_IN_BYTES);
        tmps_len += KAD_GUID_SPACE_IN_BYTES;
        iobuf_append(buf, tmps, tmps_len);
        iobuf_append(buf, (char*)compact, compact_len);
    }

    return true;
}
