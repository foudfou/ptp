#!/usr/bin/env python3
# Copyright (c) 2020 Foudil Brétel.  All rights reserved.

"""Script for single client integration tests."""

import os
import socket
import subprocess as sub
import sys
import tempfile
from typing import Dict, List, Tuple, Union

import common as c

SERVER_HOST, SERVER_AF = c.detect_ip_version()
SERVER_PORT: int = 0
SOCKET_TIMEOUT_SECONDS: float = c.DEFAULT_SOCKET_TIMEOUT

server_bin: str = sys.argv[1]
server_args: List[str] = sys.argv[2:]
server_cmd: List[str] = [
    server_bin, "-l", "error", "-a", SERVER_HOST, "-p", str(SERVER_PORT)
]  # pylint: disable=invalid-name
server_cmd.extend(server_args)


EXPECTED_MESSAGES: List[Tuple[str, bytes]] = [
    (
        "find_node",
        b"^d1:t2:(.){2}1:y1:q1:q9:find_node1:ad6:target20:(?P<id>.{20})2:id20:(?P=id)ee$"
    )
]




config_idx: int = c.find_config_option(server_cmd)
if config_idx > 0:
    print("--config option not allowed", file=sys.stderr)
    sys.exit(1)


with tempfile.TemporaryDirectory() as tmp_dir, \
     socket.socket(SERVER_AF, socket.SOCK_DGRAM) as s:
    # print(tmp_dir)
    s.settimeout(SOCKET_TIMEOUT_SECONDS)
    s.bind((SERVER_HOST, 0))
    port: int = s.getsockname()[1]
    # print(port)
    port_bytes: bytes = c.port_to_bigendian_bytes(port)

    # Create bootstrap entry with correct address format for detected IP version
    bootstrap_bin: bytes
    if SERVER_AF == socket.AF_INET6:
        # IPv6 format: 38 bytes (20 byte ID + 16 byte IPv6 + 2 byte port)
        bootstrap_bin = (
            b"l38:iam-thebootstrapnode\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1" +
            port_bytes + b"e"
        )
    else:
        # IPv4 format: 26 bytes (20 byte ID + 4 byte IPv4 + 2 byte port)
        bootstrap_bin = b"l26:iam-thebootstrapnode\x7f\0\0\1" + port_bytes + b"e"
    bootstrap_path: str = os.path.join(tmp_dir, "nodes.dat")
    with open(bootstrap_path, 'wb') as f:
        f.write(bootstrap_bin)

    server_cmd.extend(["-c", tmp_dir])
    # print(server_cmd)
    with c.managed_process(server_cmd, stderr=sub.STDOUT, close_fds=True) as server:
        BUFFER_SIZE: int = c.DEFAULT_BUFFER_SIZE
        messages_len: int = len(EXPECTED_MESSAGES)
        msg_count: int = 0
        failures: int = 0
        while msg_count < messages_len:
            data: bytes
            addr: Tuple[str, int]
            data, addr = s.recvfrom(BUFFER_SIZE)
            # print("received message: ", data)
            ctx: Dict[str, Union[int, str]] = {
                "line": msg_count,
                "len": messages_len,
                "name": EXPECTED_MESSAGES[msg_count][0]
            }
            failures += c.check_data(EXPECTED_MESSAGES[msg_count][1], data, ctx)
            msg_count += 1

retcode: int = 1 if failures else 0
sys.exit(retcode)
