#ifndef KAD_TESTS_H
#define KAD_TESTS_H

// {"t":"aa", "y":"e", "e":[201, "A Generic Error Ocurred"]}
#define KAD_TEST_ERROR "d1:eli201e23:A Generic Error Ocurrede1:t2:aa1:y1:ee"

// {"t":"aa", "y":"q", "q":"ping", "a":{"id":"abcdefghij0123456789"
#define KAD_TEST_PING_QUERY "d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t" \
    "2:aa1:y1:qe"

// {"t":"aa", "y":"r", "r": {"id":"mnopqrstuvwxyz123456"}}
#define KAD_TEST_PING_RESPONSE "d1:rd2:id20:mnopqrstuvwxyz123456e1:t2:aa" \
    "1:y1:re"

// {"t":"aa", "y":"q", "q":"find_node", "a": {"id":"abcdefghij0123456789", "target":"mnopqrstuvwxyz123456"}}
#define KAD_TEST_FIND_NODE_QUERY "d1:ad2:id20:abcdefghij01234567896:target" \
    "20:mnopqrstuvwxyz123456e1:q9:find_node1:t2:aa1:y1:qe"

// {"t":"aa", "y":"r", "r": {"id":"0123456789abcdefghij", "nodes": "def456..."}}
#define KAD_TEST_FIND_NODE_RESPONSE "d1:rd2:id20:0123456789abcdefghij5:nodesl" \
    "20:abcdefghij012345678913:192.168.168.15:12120"                    \
    "20:mnopqrstuvwxyz12345613:192.168.168.25:12121e"                   \
    "e1:t2:aa1:y1:re"

#define KAD_TEST_FIND_NODE_RESPONSE_BOGUS "d1:rd2:id20:0123456789abcdefghij" \
    "5:nodesl4:abcd13:192.168.168.15:12120ee1:t2:aa1:y1:re"

#endif /* KAD_TESTS_H */
