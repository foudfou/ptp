# Copyright (c) 2025 Foudil Brétel.  All rights reserved.

from typing import Dict, Tuple
import common as c

# Test node IDs for routes state testing
QUERY_SENDER_ID = b'jihgfedcba0123456789'
FIND_TARGET_ID = b'mnopqrstuvwxyz123456'

# name -> (message send, expected response)
MESSAGES: Dict[str, Dict[str, Tuple[bytes, bytes]]] = {
  'ip4': {
    "find_node request":
    (c.create_kad_msg_find_node(QUERY_SENDER_ID, FIND_TARGET_ID),
     b'^d1:rd2:id20:0123456789abcdefghij5:nodesl' +
     b'26:654321zyxwvutsrqponm\x7f\x00\x00\x01\x03\x04' +
     b'26:9876543210jihgfedcba\x7f\x00\x00\x01\x02\x03' +
     b'26:abcdefghij0123456789\x7f\x00\x00\x01/X' +
     b'26:mnopqrstuvwxyz123456\x7f\x00\x00\x01/Y' +
     b'ee1:t2:aa1:y1:re$'
    ),
  },
  'ip6': {
    "find_node request":
    (c.create_kad_msg_find_node(QUERY_SENDER_ID, FIND_TARGET_ID),
     b'^d1:rd2:id20:0123456789abcdefghij5:nodesl' +
     b'38:654321zyxwvutsrqponm\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\xaa\x03\x04' +
     b'38:9876543210jihgfedcba\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x02\x03' +
     b'38:abcdefghij0123456789\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01/X' +
     b'38:mnopqrstuvwxyz123456\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01/Y' +
     b'ee1:t2:aa1:y1:re$'
    ),
  },
}
