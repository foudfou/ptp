# name -> (message send, expected response)
MESSAGES = {
  'ip4': {
    "find_node request":
    (b'd1:ad2:id20:jihgfedcba01234567896:target20:mnopqrstuvwxyz123456e1:q9:find_node1:t2:aa1:y1:qe',
     b'^d1:t2:aa1:y1:r1:rd5:nodesl' +
     b'26:654321zyxwvutsrqponm\x7f\x00\x00\x01\x03\x04' +
     b'26:9876543210jihgfedcba\x7f\x00\x00\x01\x02\x03' +
     b'26:abcdefghij0123456789\x7f\x00\x00\x01/X' +
     b'26:mnopqrstuvwxyz123456\x7f\x00\x00\x01/Y' +
     b'e2:id20:0123456789abcdefghijee$'
    ),
  },
  'ip6': {
    "find_node request":
    (b'd1:ad2:id20:jihgfedcba01234567896:target20:mnopqrstuvwxyz123456e1:q9:find_node1:t2:aa1:y1:qe',
     b'^d1:t2:aa1:y1:r1:rd5:nodesl' +
     b'38:654321zyxwvutsrqponm\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\xaa\x03\x04' +
     b'38:9876543210jihgfedcba\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x02\x03' +
     b'38:abcdefghij0123456789\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01/X' +
     b'38:mnopqrstuvwxyz123456\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01/Y' +
     b'e2:id20:0123456789abcdefghijee$'
    ),
  },
}
