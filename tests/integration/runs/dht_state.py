MESSAGES = {
    # name -> (message send, expected response)
    "find_node request":
    (b'd1:ad2:id20:jihgfedcba01234567896:target20:mnopqrstuvwxyz123456e1:q9:find_node1:t2:aa1:y1:qe',
     b'^d1:t2:aa1:y1:r1:rd5:nodesl'+
     b'26:abcdefghij0123456789\xc0\xa8\xa8\x0f/X'+
     b'26:mnopqrstuvwxyz123456\xc0\xa8\xa8\x19/Y'+
     b'38:9876543210jihgfedcba\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x02\x03'+
     b'38:654321zyxwvutsrqponm\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\xaa\x03\x04'+
     b'e2:id20:0123456789abcdefghijee$'
    ),
}
