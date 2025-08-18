#!/usr/bin/env python3
# Copyright (c) 2017 Foudil Brétel.  All rights reserved.

"""Script for single server integration tests."""

import importlib
import os
import pty
import socket
import subprocess as sub
import sys
import tempfile
from contextlib import contextmanager
from typing import Dict, Generator, List, Optional, Tuple

import common as c

SERVER_HOST, SERVER_AF = c.detect_ip_version()
# SERVER_PORT is used by the server and the client. We thus can't simply bind
# the server to 0 for some random available port. Because we'd have to guess
# the port somehow for the client to connect. It's just easier to use a fixed
# value.
SERVER_PORT: int = 22220
TIMEOUT_IN_SEC: int = 1

run = importlib.import_module("runs." + sys.argv[1])

MESSAGES: Dict[str, Tuple[bytes, bytes]] = run.MESSAGES[c.get_ip_version_suffix()]

server_bin: str = sys.argv[2]
server_args: List[str] = sys.argv[3:]
server_cmd: List[str] = [
    server_bin, "-l", "error", "-a", SERVER_HOST, "-p", str(SERVER_PORT)
]  # pylint: disable=invalid-name
server_cmd.extend(server_args)

# It's generally hard to get the live output of a running subprocess in
# python. One trick is to use an intermediary fd, and use `os.read(master_fd,
# 512)`. But we want to avoid checking the logs and test server error handling
# in unit tests.
@contextmanager
def pty_pair() -> Generator[Tuple[int, int], None, None]:
    """Context manager for PTY file descriptor pairs."""
    master_fd: int
    slave_fd: int
    master_fd, slave_fd = pty.openpty()
    try:
        yield master_fd, slave_fd
    finally:
        os.close(master_fd)
        os.close(slave_fd)


def poll_data(sock: socket.socket, msg: bytes) -> Optional[bytes]:
    data_rx: Optional[bytes] = None
    # Can't reliably test that the UDP server is up and responding.
    received: Optional[bool] = None
    while received is None:
        try:
            sock.sendall(msg)
            data_rx = sock.recv(1024)
            received = True
        except ConnectionRefusedError:
            pass
        except socket.timeout:
            break
    return data_rx


config_idx: int = c.find_config_option(server_cmd)
if config_idx > 0:
    conf_dir: str = os.path.join(server_cmd[config_idx+1], c.get_ip_version_suffix())
    conf_tmp: tempfile.TemporaryDirectory[str] = tempfile.TemporaryDirectory()
    if os.path.isdir(conf_dir):
        from distutils.dir_util import copy_tree
        copy_tree(conf_dir, conf_tmp.name)
    server_cmd[config_idx+1] = conf_tmp.name

with pty_pair() as (master_fd, slave_fd), \
     c.managed_process(server_cmd, stderr=sub.STDOUT, close_fds=True) as server, \
     socket.socket(SERVER_AF, socket.SOCK_DGRAM) as s:

    s.settimeout(TIMEOUT_IN_SEC)
    s.connect((SERVER_HOST, SERVER_PORT))

    failures: int = 0
    messages_len: int = len(MESSAGES)
    i: int = 0
    for name, (msg, expect) in MESSAGES.items():
        i += 1
        data: Optional[bytes] = poll_data(s, msg)
        ctx: Dict[str, int | str] = {"line": i, "len": messages_len, "name": name}
        failures += c.check_data(expect, data, ctx)

retcode: int = c.TEST_FAIL if failures else c.TEST_OK
sys.exit(retcode)
