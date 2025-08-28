# Copyright (c) 2025 Foudil Brétel.  All rights reserved.

from typing import Dict, Tuple
import common as c

# Test node IDs for malformed message testing
SENDER_ID = c.get_test_node_id(1)
TARGET_ID = c.get_test_node_id(2)

# name -> (message send, expected response)
MESSAGES: Dict[str, Dict[str, Tuple[bytes, bytes]]] = {
    'ip4': {
        "malformed bencode - missing end":
        (b'd1:ad2:id20:' + SENDER_ID + b'e1:q4:ping1:t2:aa1:y1:q',  # missing final 'e'
         b'^d1:eli203e14:Protocol Errore1:t2:.{2}1:y1:ee$'),
        "invalid query method":
        (b'd1:ad2:id20:' + SENDER_ID + b'e1:q11:bogus_method1:t2:aa1:y1:qe',
         b'^d1:eli203e14:Protocol Errore1:t2:.{2}1:y1:ee$'),
        "oversized transaction id":
        (c.create_kad_msg_ping(SENDER_ID, b"oversized_tx_id_too_long"),
         b'^d1:eli203e14:Protocol Errore1:t2:.{2}1:y1:ee$'),
        "empty query method":
        (b'd1:ad2:id20:' + SENDER_ID + b'e1:q0:1:t2:aa1:y1:qe',
         b'^d1:eli203e14:Protocol Errore1:t2:aa1:y1:ee$'),
    },
    'ip6': {
        "malformed bencode - missing end":
        (b'd1:ad2:id20:' + SENDER_ID + b'e1:q4:ping1:t2:aa1:y1:q',  # missing final 'e'
         b'^d1:eli203e14:Protocol Errore1:t2:.{2}1:y1:ee$'),
        "invalid query method":
        (b'd1:ad2:id20:' + SENDER_ID + b'e1:q11:bogus_method1:t2:aa1:y1:qe',
         b'^d1:eli203e14:Protocol Errore1:t2:.{2}1:y1:ee$'),
        "oversized transaction id":
        (c.create_kad_msg_ping(SENDER_ID, b"oversized_tx_id_too_long"),
         b'^d1:eli203e14:Protocol Errore1:t2:.{2}1:y1:ee$'),
        "empty query method":
        (b'd1:ad2:id20:' + SENDER_ID + b'e1:q0:1:t2:aa1:y1:qe',
         b'^d1:eli203e14:Protocol Errore1:t2:aa1:y1:ee$'),
    },
}
