/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <assert.h>
#include "file.h"
#include "net/socket.h"
#include "utils/array.h"
#include "utils/safer.h"
#include "kad/test_util.h"
#include "net/kad/dht.c"      // testing static functions

KAD_TEST_NODES_DECL;

bool files_eq(const char f1[], const char f2[]) {
    char buf1[256];
    size_t buf1_len = 0;
    if (!file_read(buf1, &buf1_len, f1)) return false;
    char buf2[256];
    size_t buf2_len = 0;
    if (!file_read(buf2, &buf2_len, f2)) return false;
    return buf1_len == buf2_len && memcmp(buf1, buf2, buf1_len) == 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stdout, "Missing SOURCE_DIR argument\n");
        exit(1);
    }
    char *source_dir = argv[1];


    assert(kad_bucket_hash(&(kad_guid){.bytes = {0}}, &(kad_guid){.bytes = {0}})
           == -1);
    assert(kad_bucket_hash(
               &(kad_guid){.bytes = {[0]=0xff, [1]=0x0}, .is_set = true},
               &(kad_guid){.bytes = {[0]=0x7f, [1]=0x0}, .is_set = true})
           == KAD_GUID_SPACE_IN_BITS - 1);
    assert(kad_bucket_hash(
               &(kad_guid){.bytes = {[0]=0xff, [1]=0x0}, .is_set = true},
               &(kad_guid){.bytes = {[0]=0x8f, [1]=0x0}, .is_set = true})
           == KAD_GUID_SPACE_IN_BITS - 2);
    assert(kad_bucket_hash(
               &(kad_guid){.bytes = {[0]=0xff, [1]=0x04}, .is_set = true},
               &(kad_guid){.bytes = {[0]=0xff, [1]=0x02}, .is_set = true})
           == KAD_GUID_SPACE_IN_BITS - 14);

    size_t bkt_idx = kad_bucket_hash(
        &(kad_guid){.bytes = {[KAD_GUID_SPACE_IN_BYTES-1]=0x0}, .is_set = true},
        &(kad_guid){.bytes = {[KAD_GUID_SPACE_IN_BYTES-1]=0x1}, .is_set = true});
    assert(bkt_idx == 0);
    bkt_idx = kad_bucket_hash(
        &(kad_guid){.bytes = {[KAD_GUID_SPACE_IN_BYTES-1]=0x1}, .is_set = true},
        &(kad_guid){.bytes = {[KAD_GUID_SPACE_IN_BYTES-1]=0x1}, .is_set = true});
    assert(bkt_idx == 0);
    bkt_idx = kad_bucket_hash(
        &(kad_guid){.bytes = {[KAD_GUID_SPACE_IN_BYTES-1]=0x8}, .is_set = true},
        &(kad_guid){.bytes = {[KAD_GUID_SPACE_IN_BYTES-1]=0x1}, .is_set = true});
    assert(bkt_idx == 3);

    bkt_idx = kad_bucket_hash(
        &(kad_guid){.bytes = {[0]=0xff, [1]=0x0}, .is_set = true},
        &(kad_guid){.bytes = {[0]=0x7f, [1]=0x0}, .is_set = true});
    assert(bkt_idx == KAD_GUID_SPACE_IN_BITS-1);

    assert(!kad_guid_eq(
        &(kad_guid){.bytes = {[KAD_GUID_SPACE_IN_BYTES-1]=0x0}},
        &(kad_guid){.bytes = {[KAD_GUID_SPACE_IN_BYTES-1]=0x1}}));
    assert(!kad_guid_eq(
        &(kad_guid){.bytes = "abcdefghij0123456789"},
        &(kad_guid){.bytes = "\x00""bcdefghij0123456789"}));
    assert(kad_guid_eq(
        &(kad_guid){.bytes = "\x00""bcdefghij0123456789"},
        &(kad_guid){.bytes = "\x00""bcdefghij0123456789"}));

    assert(log_init(LOG_TYPE_STDOUT, LOG_UPTO(LOG_CRIT)));
    struct kad_dht *dht = dht_create();

    struct kad_node_info info = { .id = {.bytes = {[KAD_GUID_SPACE_IN_BYTES-1]=0x0}, .is_set = true}, {0} };
    struct sockaddr_in *sa = (struct sockaddr_in*)&info.addr;
    sa->sin_family=AF_INET; sa->sin_port=htons(0x0016); sa->sin_addr.s_addr=htonl(0x01020304);
    assert(sockaddr_storage_fmt(info.addr_str, &info.addr));
    assert(strcmp(info.addr_str, "1.2.3.4/22") == 0);

    assert(!dht_delete(dht, &info.id));
    assert(!dht_update(dht, &info, 0));
    assert(dht_insert(dht, &info, 0));
    assert(!dht_insert(dht, &info, 0));
    bkt_idx = kad_bucket_hash(&dht->self_id, &info.id);
    struct kad_node *n1 = dht_get_from_list(&dht->buckets[bkt_idx], &info.id);
    assert(n1);

    sa->sin_family=AF_INET; sa->sin_port=htons(0x0800); sa->sin_addr.s_addr=htonl(0x04030201);
    assert(dht_update(dht, &info, 0));

    assert(dht_delete(dht, &info.id));
    n1 = dht_get_from_list(&dht->buckets[bkt_idx], &info.id);
    assert(!n1);

    // insert duplicate
    info.id = dht->self_id;
    assert(!dht_insert(dht, &info, 0));

    // bucket full
    struct kad_node_info opp;
    opp.id = dht->self_id;
    opp.id.bytes[0] ^= 0x80;
    opp.id.bytes[KAD_GUID_SPACE_IN_BYTES-1] = 0;
    assert(KAD_K_CONST <= 0xff);  // avoid overflow
    for (int i = 0; i < KAD_K_CONST + 1; ++i) {
        opp.id.bytes[KAD_GUID_SPACE_IN_BYTES-1] = i;
        assert(dht_insert(dht, &opp, 0));
    }
    size_t blk_idx = 0;
    struct kad_node *overflow = dht_get_with_bucket(dht, &opp.id, NULL, &blk_idx);
    assert(overflow);
    assert(!list_is_empty(&dht->replacements[blk_idx]));

    // mark stale
    assert(dht_mark_stale(dht, &overflow->info.id));
    assert(overflow->stale == 1);

    dht_destroy(dht);


    char path[256];
    snprintf(path, 256, "%s/%s", source_dir, "tests/kad/data/dht.dat");
    path[255] = '\0';
    assert(access(path, R_OK) != -1 );

    int nread = dht_read(&dht, path);
    assert(nread == ARRAY_LEN(kad_test_nodes));
    assert(dht);
    assert(memcmp(dht->self_id.bytes, "0123456789abcdefghij5", KAD_GUID_SPACE_IN_BYTES) == 0);
    for (size_t i=0; i<ARRAY_LEN(kad_test_nodes); i++) {
        const struct kad_node *knode =
            dht_get_with_bucket(dht, &kad_test_nodes[i].id, NULL, NULL);
        assert(knode);
        assert(kad_node_info_equals(&knode->info, &kad_test_nodes[i]));
    }

    char tpl[] = "/tmp/tmpXXXXXX";
    assert(mkdtemp(tpl));
    snprintf(path, 255, "%s/%s", tpl, "dht.dat");
    assert(dht_write(dht, path));
    char ref[256];
    snprintf(ref, 255, "%s/%s", source_dir, "tests/kad/data/dht_sorted.dat");
    assert(files_eq(path, ref));
    assert(!remove(path));
    assert(!remove(tpl));


    dht_destroy(dht);

    log_shutdown(LOG_TYPE_STDOUT);

    return 0;
}
