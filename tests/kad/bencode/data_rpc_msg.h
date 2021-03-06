/* Copyright (c) 2019 Foudil Brétel.  All rights reserved. */
#ifndef KAD_TEST_RPC_MSG_H
#define KAD_TEST_RPC_MSG_H

// {"t":"aa", "y":"e", "e":[201, "A Generic Error Ocurred"]}
#define KAD_TEST_ERROR "d1:eli201e23:A Generic Error Ocurrede1:t2:aa1:y1:ee"

// {"t":"aa", "y":"q", "q":"ping", "a":{"id":"abcdefghij0123456789"
#define KAD_TEST_PING_QUERY "d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t" \
    "2:aa1:y1:qe"

// {"t":"aa", "y":"r", "r": {"id":"mnopqrstuvwxyz123456"}}
#define KAD_TEST_PING_RESPONSE "d1:rd2:id20:mnopqrstuvwxyz123456e1:t2:aa" \
    "1:y1:re"

#define KAD_TEST_PING_RESPONSE_BIN_ID "d1:t2:aa1:y1:r1:rd2:id20:\x17\x45" \
    "\xc4\xed\xca\x16\x33\xf0\x51\x8e\x1f\x36\x0a\xc7\xe1\xad\x27\x41" \
    "\x86\x33""ee"

// {"t":"aa", "y":"q", "q":"find_node", "a": {"id":"abcdefghij0123456789", "target":"mnopqrstuvwxyz123456"}}
#define KAD_TEST_FIND_NODE_QUERY "d1:ad2:id20:abcdefghij01234567896:target" \
    "20:mnopqrstuvwxyz123456e1:q9:find_node1:t2:aa1:y1:qe"

// {"t":"aa", "y":"q", "q":"find_node", "x": {"id":"abcdefghij0123456789", "target":"mnopqrstuvwxyz123456"}}
#define KAD_TEST_FIND_NODE_QUERY_BOGUS "d1:xd2:id20:abcdefghij01234567896:target" \
    "20:mnopqrstuvwxyz123456e1:q9:find_node1:t2:aa1:y1:qe"

// {"t":"aa", "y":"r", "r": {"id":"0123456789abcdefghij", "nodes": "def456..."}}
#define KAD_TEST_FIND_NODE_RESPONSE "d1:rd2:id20:0123456789abcdefghij5:nodesl" \
    "26:abcdefghij0123456789\xc0\xa8\xa8\x0f\x2f\x58"                   \
    "26:mnopqrstuvwxyz123456\xc0\xa8\xa8\x19\x2f\x59"                   \
    "ee1:t2:aa1:y1:re"

#define KAD_TEST_FIND_NODE_RESPONSE_IP6 "d1:rd2:id20:0123456789abcdefghij5:nodesl" \
    "38:abcdefghij0123456789\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x02\x03" \
    "38:mnopqrstuvwxyz123456\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\xaa\x03\x04" \
    "ee1:t2:aa1:y1:re"

#define KAD_TEST_FIND_NODE_RESPONSE_BOGUS "d1:rd2:id20:0123456789abcdefghij" \
    "5:nodesl10:abcd\xc0\xa8\xa8\x0f\x2f\x58""ee1:t2:aa1:y1:re"

#endif /* KAD_TEST_RPC_MSG_H */
