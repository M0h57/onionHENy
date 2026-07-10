#!/usr/bin/env python3
"""Deprecated alias — use shutdown_orion.py.

Historically pointed at TCP :9048; now triggers full stack shutdown.
"""

from __future__ import annotations

import sys
from pathlib import Path

# Re-export new entrypoint
sys.path.insert(0, str(Path(__file__).resolve().parent))
from shutdown_orion import main  # noqa: E402

if __name__ == "__main__":
    main()
