/* Copyright (c) 2017-2019 Foudil Brétel.  All rights reserved. */
#ifndef BENCODE_DHT_H
#define BENCODE_DHT_H

#include <stdbool.h>
#include "net/iobuf.h"
#include "net/kad/dht.h"

bool benc_decode_dht(struct kad_dht_encoded *dht, const char buf[], const size_t slen);
bool benc_encode_dht(struct iobuf *buf, const struct kad_dht_encoded *dht);

#endif /* BENCODE_DHT_H */
