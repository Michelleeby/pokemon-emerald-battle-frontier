#!/usr/bin/env python3
"""Minimal protocol peer used only by host-side Session unit tests."""

from __future__ import annotations

import sys
import time


print("READY protocol=1 frame=0", flush=True)
for line in sys.stdin:
    command = line.strip()
    if command == "HANG":
        time.sleep(60)
    elif command == "CRASH":
        sys.exit(17)
    elif command == "QUIT":
        print("OK bye", flush=True)
        break
    else:
        print("OK pong", flush=True)
