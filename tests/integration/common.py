#!/usr/bin/env python3
# Copyright (c) 2020 Foudil Brétel.  All rights reserved.

"""Common utilities and constants for integration tests."""

import socket
import sys

# Bencode parsing constants
BENCODE_HEADER_SIZE = 36
BENCODE_FOOTER_SIZE = 2
NODE_ENTRY_COMPACT_SIZE = 26
NODE_ENTRY_FULL_SIZE = 38

# Default test parameters
DEFAULT_SOCKET_TIMEOUT = 1.0
DEFAULT_SERVER_HOST = '::1'
DEFAULT_SERVER_AF = socket.AF_INET6
DEFAULT_BUFFER_SIZE = 1024

def port_to_bigendian_bytes(port):
    """Convert port number to big-endian bytes representation."""
    return port.to_bytes((port.bit_length() + 7) // 8, 'big') or b'\0'

def find_config_option(server_cmd):
    """
    Find config option in server command.
    
    Returns index of config option, or -1 if not found.
    """
    try:
        return server_cmd.index('-c')
    except ValueError:
        try:
            return server_cmd.index('--config')
        except ValueError:
            return -1