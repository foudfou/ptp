#ifndef KAD_H
#define KAD_H

#include "proto/serialization.h"

/* #define KAD_GUID_SPACE 64 */

/* typedef union u64 kad_guid; */

/* TESTING: BEGIN */
#define KAD_GUID_SPACE 4

struct uint4 {
    uint8_t hi : 4;
    uint8_t dd : 4;
};

typedef struct uint4 kad_guid;
/* TESTING: END */

kad_guid kad_init();
void kad_shutdown();
kad_guid kad_generate_id();

#endif /* KAD_H */
