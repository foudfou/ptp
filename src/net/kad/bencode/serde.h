/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#ifndef BENCODE_KAD_H
#define BENCODE_KAD_H

/**
 * Definitions common to multiple de-/serializers.
 */
#include <stdbool.h>
#include "net/iobuf.h"
#include "net/kad/routes.h"
#include "utils/lookup.h"

// "Compact node info"
#define BENC_IP4_ADDR_LEN_IN_BYTES 4
#define BENC_IP6_ADDR_LEN_IN_BYTES 16
#define BENC_KAD_NODE_INFO_IP4_LEN_IN_BYTES KAD_GUID_SPACE_IN_BYTES + BENC_IP4_ADDR_LEN_IN_BYTES + 2
#define BENC_KAD_NODE_INFO_IP6_LEN_IN_BYTES KAD_GUID_SPACE_IN_BYTES + BENC_IP6_ADDR_LEN_IN_BYTES + 2

const struct benc_node*
benc_node_find_literal_str(const struct benc_node *dict,
                           const char key[], const size_t key_len);
const struct benc_node*
benc_node_navigate_to_key(const struct benc_node *dict,
                          const lookup_entry k_names[],
                          const int k1, const int k2);
int benc_read_nodes(struct kad_node_info nodes[], const size_t nodes_len,
                    const struct benc_node *list);
int benc_read_nodes_from_key(struct kad_node_info nodes[], const size_t nodes_len,
                             const struct benc_node *dict,
                             const lookup_entry k_names[],
                             const int k1, const int k2);
bool benc_read_guid(kad_guid *id, const struct benc_literal *lit);
bool benc_write_nodes(struct iobuf *buf, const struct kad_node_info nodes[], size_t nodes_len);

#endif /* BENCODE_KAD_H */
