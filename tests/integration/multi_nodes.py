#!/usr/bin/env python3
# Copyright (c) 2020 Foudil Brétel.  All rights reserved.

"""
Script for multiple agents integration tests.

Accepts a configuration name from runs/x.py
Usage: multi_nodes.py <config_name> <binary> [args...]
"""

import os
import time
import socket
import subprocess as sub
import sys
import tempfile
from typing import Any, Dict, Generator, List, Optional, Tuple

from contextlib import contextmanager

import common as c
from runs.multi_nodes import CONFIGS


DEBUG: bool = False

run = sys.argv[1]
if run in ["scale_5", "scale_10"]: # SKIPPED for performance
    sys.exit(77)
CONFIG: Dict[str, Any] = CONFIGS[run]
NODES_LEN: int = CONFIG['nodes_len']
SLEEP_BOOTSTRAP_NODE_READY: float = CONFIG['sleep_bootstrap_ready']
SLEEP_NODES_FINISH: float = CONFIG['sleep_nodes_finish']

# Use standard test node IDs from common module
NODES: List[bytes] = [c.get_test_node_id(i) for i in range(NODES_LEN)]

SERVER_HOST, SERVER_AF = c.detect_ip_version()
SOCKET_TIMEOUT_SECONDS: float = c.DEFAULT_SOCKET_TIMEOUT

path: str = os.path.abspath(os.path.join(os.path.dirname(__file__)))  # pylint: disable=invalid-name

server_bin: str = sys.argv[2]
server_args: List[str] = sys.argv[3:]
server_cmd: List[str] = [
    server_bin, "-l", "debug" if DEBUG else "error", "-a", SERVER_HOST
]  # pylint: disable=invalid-name
server_cmd.extend(server_args)


def get_free_port() -> int:
    """
    Determines a free port using sockets.

    https://stackoverflow.com/a/47964377 but has issues:
    https://eklitzke.org/binding-on-port-zero
    Works as long as set SO_REUSEADDR in server.
    """
    with socket.socket(SERVER_AF, socket.SOCK_DGRAM) as free_socket:
        free_socket.bind((SERVER_HOST, 0))
        port: int = free_socket.getsockname()[1]
        return port


def create_node(
    cmd: List[str],
    tmp_dir: str,
    nodeid: bytes,
    bootstrap: Optional[int] = None
) -> Tuple[sub.Popen[bytes], int]:
    routes_bin: bytes = c.create_empty_routing_table(nodeid)
    routes_path: str = os.path.join(tmp_dir, "routes.dat")
    with open(routes_path, 'wb') as f:
        f.write(routes_bin)

    if bootstrap is not None:
        # Create bootstrap entry with correct address format for detected IP version
        bootstrap_bin: bytes = c.create_bootstrap_entry(
            c.get_test_node_id(0), bootstrap, SERVER_AF  # Use bootstrap node ID
        )
        bootstrap_path: str = os.path.join(tmp_dir, "nodes.dat")
        with open(bootstrap_path, 'wb') as f:
            f.write(bootstrap_bin)

    port: int = get_free_port()
    cmd.extend(["-p", str(port), "-c", tmp_dir])

    server: sub.Popen[bytes] = sub.Popen(
        cmd, stderr=sub.STDOUT, close_fds=True, stdout=sub.PIPE)
    return (server, port)


@contextmanager
def tmpdirs(count: int) -> Generator[List[tempfile.TemporaryDirectory[str]], None, None]:
    dirs: List[tempfile.TemporaryDirectory[str]] = [
        tempfile.TemporaryDirectory() for i in range(count)
    ]
    try:
        yield dirs
    finally:
        for d in dirs:
            d.cleanup()



config_idx: int = c.find_config_option(server_cmd)
if config_idx > 0:
    print("--config option not allowed", file=sys.stderr)
    sys.exit(1)


with tmpdirs(NODES_LEN) as tmp_dirs:
    dirs: List[str] = [d.name for d in tmp_dirs]
    instances: List[Tuple[sub.Popen[bytes], int]] = []

    try:
        bootnode: sub.Popen[bytes]
        bootnode_port: int
        (bootnode, bootnode_port) = create_node(
            server_cmd.copy(), tmp_dir=dirs[0],
            nodeid=NODES[0]
        )
        instances.append((bootnode, bootnode_port))

        time.sleep(SLEEP_BOOTSTRAP_NODE_READY)

        for i in range(1, NODES_LEN):
            instances.append(create_node(
                server_cmd.copy(), tmp_dir=dirs[i], bootstrap=bootnode_port,
                nodeid=NODES[i]
            ))

        time.sleep(SLEEP_NODES_FINISH)

    finally:
        # Clean up processes
        for server, port in reversed(instances):
            server.terminate()
            server.wait(timeout=5)

    # Read routes after processes have been terminated
    routes: List[bytes] = []
    for i in range(NODES_LEN):
        with open(dirs[i] + "/routes.dat", 'rb') as fd:
            routes.append(fd.read())

    failures: int = 0

    def extract_routes(me: bytes, data: bytes) -> set[bytes]:
        ret: set[bytes] = set()

        if data[0:c.BENCODE_HEADER_SIZE] != b'd2:id20:' + me + b'5:nodesl' or \
           data[-c.BENCODE_FOOTER_SIZE:] != b'ee':
            return ret

        nodes: bytes = data[c.BENCODE_HEADER_SIZE:-c.BENCODE_FOOTER_SIZE]

        node_ids: List[bytes] = []
        i: int = 0
        while len(nodes) - c.NODE_ENTRY_COMPACT_SIZE > i:
            length: bytes = nodes[i:i+3]
            j: int
            if length == b'26:':
                j = c.NODE_ENTRY_COMPACT_SIZE
            elif length == b'38:':
                j = c.NODE_ENTRY_FULL_SIZE
            else:
                return ret
            i += 3
            node_ids.append(nodes[i:i+20])
            i += j

        return set(node_ids)

    nodes_set: set[bytes] = set(NODES)
    for i in range(NODES_LEN):
        node_routes: set[bytes] = extract_routes(NODES[i], routes[i])
        result: str
        data: Optional[bytes]
        if node_routes == nodes_set - {NODES[i]}:
            result = "OK"
            data = None
        else:
            failures += 1
            result = "\033[31mFAIL\033[0m"
            data = routes[i]
        print(f"{i+1}/{NODES_LEN} routes_{i} .. {result}")
        if data:
            print(f"---\n   incorrect data {data!r}")

retcode: int = 1 if failures else 0
sys.exit(retcode)
