MESSAGES = {
    # name -> (message send, expected response)
    "ping request":
    (b'd1:ad2:id20:\x16\x01\xfb\xa7\xca\x18P\x9e\xe5\x1d\xcc\x0e\xf8\xc6Z\x1a\xfe<s\x81e1:q4:ping1:t2:aa1:y1:qe',
     b'^d1:t2:aa1:y1:r1:rd2:id20:.{20}ee$'),
    "ping request missing id":
    (b'd1:ade1:q4:ping1:t2:XX1:y1:qe', b'^d1:t2:XX1:y1:e1:eli203e14:Protocol Erroree$'),
    "find_node request":
    (b'd1:ad2:id20:abcdefghij01234567896:target20:mnopqrstuvwxyz123456e1:q9:find_node1:t2:aa1:y1:qe',
     b'^d1:t2:aa1:y1:r1:rd5:nodesl(26|38):.*ee$'),
}
