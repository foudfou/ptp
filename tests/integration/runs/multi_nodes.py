# Copyright (c) 2025 Foudil Brétel.  All rights reserved.

from typing import Dict, Any

CONFIGS: Dict[str, Dict[str, Any]] = {
    'bootstrap': {
        'nodes_len': 3,
        'sleep_bootstrap_ready': 0.02,
        'sleep_nodes_finish': 0.3,
    },
    'scale_5': {
        'nodes_len': 5,
        'sleep_bootstrap_ready': 0.05,
        'sleep_nodes_finish': 1.0,
    },
    'scale_10': {
        'nodes_len': 10,
        'sleep_bootstrap_ready': 0.1,
        'sleep_nodes_finish': 3.0,
    }
}
