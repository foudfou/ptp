#!/usr/bin/env python3
# Copyright (c) 2020 Foudil Brétel.  All rights reserved.

"""Common utilities and constants for integration tests."""

import os
import re
import socket
import subprocess as sub
from contextlib import contextmanager
from typing import Dict, Generator, List, Optional, Tuple, Union

# Meson test convention https://mesonbuild.com/Unit-tests.html
TEST_OK   = 0
TEST_FAIL = 1
TEST_SKIP = 77

# Bencode parsing constants
BENCODE_HEADER_SIZE = 36
BENCODE_FOOTER_SIZE = 2
NODE_ENTRY_COMPACT_SIZE = 26
NODE_ENTRY_FULL_SIZE = 38

# Default test parameters
DEFAULT_SOCKET_TIMEOUT = 1.0
DEFAULT_BUFFER_SIZE = 1024

def detect_ip_version() -> Tuple[str, int]:
    """
    Detect available IP version and return appropriate host/family.

    Returns tuple: (host, address_family)
    Prefers IPv6 if available, falls back to IPv4.
    """
    try:
        with socket.socket(socket.AF_INET6, socket.SOCK_DGRAM) as s:
            s.bind(('::1', 0))
            return ('::1', socket.AF_INET6)
    except (socket.error, OSError):
        return ('127.0.0.1', socket.AF_INET)

def get_ip_version_suffix() -> str:
    _, af = detect_ip_version()
    return 'ip6' if af == socket.AF_INET6 else 'ip4'

def port_to_bigendian_bytes(port: int) -> bytes:
    """Convert port number to big-endian bytes representation."""
    return port.to_bytes((port.bit_length() + 7) // 8, 'big') or b'\0'

def find_config_option(cmd: List[str]) -> int:
    """
    Find config option in server command.

    Returns index of config option, or -1 if not found.
    """
    try:
        return cmd.index('-c')
    except ValueError:
        try:
            return cmd.index('--config')
        except ValueError:
            return -1

def check_data(expect: bytes, data: Optional[bytes], ctx: Dict[str, Union[int, str]]) -> int:
    """
    Check if data matches expected pattern and print test result.

    Args:
        expect: Expected pattern (regex bytes)
        data: Actual data received (bytes or None)
        ctx: Test context dict with 'line', 'len', 'name' keys

    Returns:
        Number of failures (0 or 1)
    """
    failure = 0
    patt = re.compile(expect, re.DOTALL)
    if data is not None and re.search(patt, data):
        result = "OK"
        err = None
    else:
        failure += 1
        result = "\033[31mFAIL\033[0m"
        err = data
    print(f"{ctx['line']}/{ctx['len']} {ctx['name']} .. {result}")
    if err:
        print(f"---\n   data {data!r} did not match expected {expect!r}")
    return failure


# Standard test node IDs used across tests
TEST_NODE_IDS = [
    b"1234567890abcdefghij",  # 0x3132, bootstrap node
    b"abcdefghij1234567890",  # 0x6162
    b"mnopqrstuv0987654321",  # 0x6D6E
]

def get_test_node_id(index: int) -> bytes:
    """Get standard test node ID by index."""
    if index < len(TEST_NODE_IDS):
        return TEST_NODE_IDS[index]
    # Generate deterministic IDs for indices beyond predefined
    import hashlib
    return hashlib.sha1(f"test_node_{index}".encode()).digest()

def create_empty_routing_table(node_id: bytes) -> bytes:
    """Create empty routing table for given node."""
    return b"d2:id20:" + node_id + b"5:nodeslee"


def create_bootstrap_entry(node_id: bytes, port: int, ip_version: int) -> bytes:
    """Create a bootstrap nodes.dat entry for given node ID and port."""
    port_bytes: bytes = port_to_bigendian_bytes(port)

    if ip_version == socket.AF_INET6:
        # IPv6 format: 38 bytes (20 byte ID + 16 byte IPv6 + 2 byte port)
        return (
            b"l38:" + node_id + b"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1" +
            port_bytes + b"e"
        )
    else:
        # IPv4 format: 26 bytes (20 byte ID + 4 byte IPv4 + 2 byte port)
        return b"l26:" + node_id + b"\x7f\0\0\1" + port_bytes + b"e"

def create_kad_msg_ping(node_id: bytes, tx_id: bytes = b"aa") -> bytes:
    """Create a Kademlia ping RPC message."""
    return (
        b'd1:ad2:id20:' + node_id + b'e1:q4:ping1:t2:' + tx_id + b'1:y1:qe'
    )

def create_kad_msg_find_node(node_id: bytes, target: bytes, tx_id: bytes = b"aa") -> bytes:
    """Create a Kademlia find_node RPC message."""
    return (
        b'd1:ad2:id20:' + node_id + b'6:target20:' + target +
        b'e1:q9:find_node1:t2:' + tx_id + b'1:y1:qe'
    )

def create_kad_msg_ping_bogus_missing_id(tx_id: bytes = b"XX") -> bytes:
    """Create a malformed ping message missing the node ID for error testing."""
    return b'd1:ade1:q4:ping1:t2:' + tx_id + b'1:y1:qe'


@contextmanager
def managed_process(cmd: List[str], **kwargs) -> Generator[sub.Popen[bytes], None, None]:
    """Context manager for subprocess.Popen with proper cleanup."""
    process: sub.Popen[bytes] = sub.Popen(cmd, **kwargs)
    try:
        yield process
    finally:
        process.terminate()
        process.wait(timeout=5)
