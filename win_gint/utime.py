# -*- coding: utf-8 -*-
"""
utime.py -- MicroPython / PythonExtra `utime` & `time` module for Windows IDLE.

Note: `import gint` already upgrades Python's built-in `time` module automatically
with `ticks_ms`, `ticks_us`, `ticks_cpu`, `ticks_add`, `ticks_diff`, `sleep_ms`,
and `sleep_us`. This file is provided so scripts using `import utime` or
`from utime import ticks_ms, sleep_ms` also work in Windows IDLE out of the box!
"""

from time import *  # noqa: F401,F403
import gint as _gint

ticks_ms = _gint.ticks_ms
ticks_us = _gint.ticks_us
ticks_cpu = _gint.ticks_cpu
ticks_add = _gint.ticks_add
ticks_diff = _gint.ticks_diff
sleep_ms = _gint.sleep_ms
sleep_us = _gint.sleep_us
