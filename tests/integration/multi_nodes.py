#!/usr/bin/env python3
# Copyright (c) 2020 Foudil Brétel.  All rights reserved.

"""Script for multiple agents integration tests.

Starts a bootstrap node then multiple other nodes. Let their DHT converge and
assess the results.
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


DEBUG: bool = True

run = sys.argv[1]
if run in ["scale_5", "scale_10"]: # skipped for performance
    sys.exit(c.TEST_SKIP)
CONFIG: Dict[str, Any] = CONFIGS[run]
NODES_LEN: int = CONFIG['nodes_len']
SLEEP_BOOTSTRAP_NODE_READY: float = CONFIG['sleep_bootstrap_ready']
SLEEP_NODES_FINISH: float = CONFIG['sleep_nodes_finish']

SERVER_HOST, SERVER_AF = c.detect_ip_version()
SOCKET_TIMEOUT_SECONDS: float = c.DEFAULT_SOCKET_TIMEOUT

# Use standard test node IDs from common module
nodes: List[bytes] = [c.get_test_node_id(i) for i in range(NODES_LEN)]

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
    bootstrap: Optional[int] = None,
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

    out_path: str = os.path.join(tmp_dir, "stdout")
    err_path: str = os.path.join(tmp_dir, "stderr")
    server: sub.Popen[bytes]
    with open(out_path, 'w') as out_file, open(err_path, 'w') as err_file:
        server = sub.Popen(
            cmd, close_fds=True, stderr=err_file, stdout=out_file, text=True)
    return (server, port)


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

    import datetime
    try:
        # Start bootstrap node
        bootnode: sub.Popen[bytes]
        (bootnode, bootnode_port) = create_node(
            server_cmd.copy(), tmp_dir=dirs[0],
            nodeid=nodes[0],
        )
        print(f"__{datetime.datetime.now()} → bootstrap launched")
        instances.append((bootnode, bootnode_port))

        time.sleep(SLEEP_BOOTSTRAP_NODE_READY)

        # Start N other nodes
        for i in range(1, NODES_LEN):
            instances.append(create_node(
                server_cmd.copy(), tmp_dir=dirs[i], bootstrap=bootnode_port,
                nodeid=nodes[i]
            ))
            print(f"__{datetime.datetime.now()} → node({nodes[i].hex()}) launched")

        time.sleep(SLEEP_NODES_FINISH)

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)

    finally:
        # Clean up processes - terminate all and wait for them to finish
        for i, (server, port) in enumerate(instances):
            server.terminate()
            print(f"__{datetime.datetime.now()} → {'bootstrap' if i == 0 else f'node[{i}]'} terminated")
            server.wait(timeout=5)

    # Give a moment for file buffers to flush
    time.sleep(0.1)

    # Print output from last node (which tends to have issues)
    K: int = NODES_LEN - 2

    out_path: str = os.path.join(dirs[K], "stdout")
    err_path: str = os.path.join(dirs[K], "stderr")

    try:
        with open(out_path, 'r') as f:
            print(f"\n--- node[{K}] stdout ---")
            print(f.read())
            print(f"--- End node[{K}] stdout ---\n")
    except FileNotFoundError:
        print(f"\n--- node[{K}] stdout file not found ---\n")

    try:
        with open(err_path, 'r') as f:
            print(f"\n--- node[{K}] stderr ---")
            print(f.read())
            print(f"--- End node[{K}] stderr ---\n")
    except FileNotFoundError:
        print(f"\n--- node[{K}] stderr file not found ---\n")

    # Read routes after processes have been terminated
    routes: List[bytes] = []
    for i in range(NODES_LEN):
        with open(dirs[i] + "/routes.dat", 'rb') as fd:
            routes.append(fd.read())

    print(f"__node[{K}] routes")
    print(extract_routes(nodes[K], routes[K]))
    print(f"__End node[{K}] routes")

    failures: int = 0

    # Expect each know all
    nodes_set: set[bytes] = set(nodes)
    for i in range(NODES_LEN):
        node_routes: set[bytes] = extract_routes(nodes[i], routes[i])
        result: str = "OK"
        data: Optional[bytes] = None
        if node_routes != nodes_set - {nodes[i]}:
            failures += 1
            result = "\033[31mFAIL\033[0m"
            data = routes[i]
        print(f"{i+1}/{NODES_LEN} routes_{i} .. {result}")
        data = routes[i]
        print(f"---\n   data {data!r}")
        # if data:
        #     print(f"---\n   incorrect data {data!r}")

retcode: int = c.TEST_FAIL if failures else c.TEST_OK
sys.exit(retcode)
