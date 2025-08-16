#!/usr/bin/env python3
# Copyright (c) 2020 Foudil Brétel.  All rights reserved.

"""Common utilities and constants for integration tests."""

import os
import re
import socket
import subprocess as sub
from contextlib import contextmanager
from typing import Dict, Generator, List, Optional, Tuple, Union

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


@contextmanager
def managed_process(cmd: List[str], **kwargs) -> Generator[sub.Popen[bytes], None, None]:
    """Context manager for subprocess.Popen with proper cleanup."""
    process: sub.Popen[bytes] = sub.Popen(cmd, **kwargs)
    try:
        yield process
    finally:
        process.terminate()
        process.wait(timeout=5)
