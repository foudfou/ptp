# Copyright (c) 2025 Foudil Brétel.  All rights reserved.

from typing import Dict, Tuple
import common as c

# Test node IDs for message construction
SENDER_ID = b'\x16\x01\xfb\xa7\xca\x18P\x9e\xe5\x1d\xcc\x0e\xf8\xc6Z\x1a\xfe<s\x81'
QUERY_SENDER_ID = b'abcdefghij0123456789'
FIND_TARGET_ID = b'mnopqrstuvwxyz123456'

# name -> (message send, expected response)
MESSAGES: Dict[str, Dict[str, Tuple[bytes, bytes]]] = {
    'ip4': {
        "ping request":
        (c.create_kad_msg_ping(SENDER_ID),
         b'^d1:t2:aa1:y1:r1:rd2:id20:.{20}ee$'),
        "ping request missing id":
        (c.create_kad_msg_ping_bogus_missing_id(),
         b'^d1:t2:XX1:y1:e1:eli203e14:Protocol Erroree$'),
        "find_node request":
        (c.create_kad_msg_find_node(QUERY_SENDER_ID, FIND_TARGET_ID),
         b'^d1:t2:aa1:y1:r1:rd5:nodesl26:.*ee$'),
    },
    'ip6': {
        "ping request":
        (c.create_kad_msg_ping(SENDER_ID),
         b'^d1:t2:aa1:y1:r1:rd2:id20:.{20}ee$'),
        "ping request missing id":
        (c.create_kad_msg_ping_bogus_missing_id(),
         b'^d1:t2:XX1:y1:e1:eli203e14:Protocol Erroree$'),
        "find_node request":
        (c.create_kad_msg_find_node(QUERY_SENDER_ID, FIND_TARGET_ID),
         b'^d1:t2:aa1:y1:r1:rd5:nodesl(26|38):.*ee$'),
    },
}
