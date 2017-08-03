/* Copyright (c) 2017 Foudil Brétel.  All rights reserved. */
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "utils/u64.h"

int main ()
{
    union u32 ul = {0x11223344};
    char ulstr[16] = "";
    assert(sprintf(ulstr, "%"PRIu32, ul.dd) == 9);
    assert(strcmp(ulstr, "287454020") == 0);
    assert(sprintf(ulstr, "%"PRIx32, ul.dd) == 8);
    assert(strcmp(ulstr, "11223344") == 0);

    assert(sprintf(ulstr, "%02x%02x%02x%02x",
                   ul.db[3], ul.db[2], ul.db[1], ul.db[0]) == 8);
    if (IS_LITTLE_ENDIAN)
        assert(strcmp(ulstr, "11223344") == 0);
    else
        assert(strcmp(ulstr, "44332211") == 0);

    uint32_t ulhton = htonl(ul.dd);
    assert(sprintf(ulstr, "%"PRIx32, ulhton) == 8);
    assert(strcmp(ulstr, "44332211") == 0);

    union u32 ul_hton = u32_hton(ul);
    assert(sprintf(ulstr, "%"PRIx32, ul_hton.dd) == 8);
    assert(strcmp(ulstr, "44332211") == 0);

    union u32 ul_self = u32_ntoh(ul_hton);
    assert(ul.dd == ul_self.dd);


    union u64 ull = {0x1122334455667788};
    char ullstr[32] = "";
    assert(sprintf(ullstr, "%"PRIu64, ull.dd) == 19);
    assert(strcmp(ullstr, "1234605616436508552") == 0);
    assert(sprintf(ullstr, "%"PRIx64, ull.dd) == 16);
    assert(strcmp(ullstr, "1122334455667788") == 0);

    assert(sprintf(ullstr, "%02x%02x%02x%02x%02x%02x%02x%02x",
                   ull.db[7], ull.db[6], ull.db[5], ull.db[4],
                   ull.db[3], ull.db[2], ull.db[1], ull.db[0]) == 16);
    if (IS_LITTLE_ENDIAN)
        assert(strcmp(ullstr, "1122334455667788") == 0);
    else
        assert(strcmp(ullstr, "8877665544332211") == 0);

    union u64 ull_hton = u64_hton(ull);
    assert(sprintf(ullstr, "%"PRIx64, ull_hton.dd) == 16);
    assert(strcmp(ullstr, "8877665544332211") == 0);

    union u64 ull_self = u64_ntoh(ull_hton);
    assert(ull.dd == ull_self.dd);


    return 0;
}
