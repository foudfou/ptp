/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#ifndef BENCODE_ROUTES_H
#define BENCODE_ROUTES_H

#include <stdbool.h>
#include "net/iobuf.h"
#include "net/kad/routes.h"

bool benc_decode_routes(struct kad_routes_encoded *routes, const char buf[], const size_t slen);
bool benc_encode_routes(struct iobuf *buf, const struct kad_routes_encoded *routes);

int benc_decode_bootstrap_nodes(struct sockaddr_storage nodes[],
                                const size_t nodes_len,
                                const char buf[], const size_t slen);

#endif /* BENCODE_ROUTES_H */
