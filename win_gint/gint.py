# -*- coding: utf-8 -*-
"""
gint.py -- Fast Native Windows Emulator of Casio fx-CG50 PythonExtra `gint`
           (+ automatic MicroPython `time` extensions) for Windows Python IDLE!

Zero external dependencies (uses built-in Win32 GDI32 / User32 via `ctypes`).
Double-buffered with a dedicated Win32 UI thread:
  - Main thread draws lock-free into offscreen GDI DIBSection at C speed
  - Dedicated UI thread keeps the window silky-smooth (drag/resize/zoom/close)
    even during `time.sleep()`, `input()`, or after returning to IDLE `>>>`!

PC Keyboard -> Casio fx-CG50 Mapping:
  Arrows          -> UP / DOWN / LEFT / RIGHT
  Enter           -> EXE
  Esc             -> EXIT
  Backspace / Del -> DEL
  F1 .. F6        -> F1 .. F6
  0 .. 9 (or Num) -> KEY_0 .. KEY_9
  + - * / . , [ ] -> KEY_ADD, KEY_SUB, KEY_MUL, KEY_DIV, KEY_DOT, KEY_COMMA, ( )
  ` (tilde)       -> KEY_NEG  (-)
  Shift           -> KEY_SHIFT
  Ctrl / CapsLock -> KEY_ALPHA
  Tab / Home      -> KEY_OPTN / KEY_MENU
  A .. Z          -> Casio ALPHA physical keys (type filenames naturally!)
  F9              -> Cycle window zoom (1x / 2x / 3x / 4x)
  F12             -> Save pixel-perfect screenshot (gint_shot_XXXX.bmp)
"""

from __future__ import annotations

import atexit
import builtins
import collections
import ctypes
import os
import struct
import sys
import threading
import time as _stdlib_time

# ============================================================================
# 1. AUTOMATIC MICROPYTHON / PYTHONEXTRA `time` PATCH FOR WINDOWS IDLE
# ============================================================================
_TICKS_MASK = 0x3FFFFFFF
_TICKS_HALF = 0x20000000
_T0_NS = _stdlib_time.perf_counter_ns()
_orig_sleep = _stdlib_time.sleep
_orig_open = builtins.open


def ticks_ms() -> int:
    return ((_stdlib_time.perf_counter_ns() - _T0_NS) // 1_000_000) & _TICKS_MASK


def ticks_us() -> int:
    return ((_stdlib_time.perf_counter_ns() - _T0_NS) // 1_000) & _TICKS_MASK


def ticks_cpu() -> int:
    return (_stdlib_time.perf_counter_ns() - _T0_NS) & _TICKS_MASK


def ticks_add(ticks: int, delta: int) -> int:
    return (int(ticks) + int(delta)) & _TICKS_MASK


def ticks_diff(ticks1: int, ticks2: int) -> int:
    return ((int(ticks1) - int(ticks2) + _TICKS_HALF) & _TICKS_MASK) - _TICKS_HALF


def sleep_ms(ms: int) -> None:
    if ms > 0:
        _orig_sleep(ms * 0.001)


def sleep_us(us: int) -> None:
    if us <= 0:
        return
    if us >= 2000:
        _orig_sleep((us // 1000) * 0.001)
        us = us % 1000
    target = _stdlib_time.perf_counter_ns() + us * 1000
    while _stdlib_time.perf_counter_ns() < target:
        pass


def _patch_stdlib_time() -> None:
    if not hasattr(_stdlib_time, "ticks_ms"):
        _stdlib_time.ticks_ms = ticks_ms          # type: ignore[attr-defined]
        _stdlib_time.ticks_us = ticks_us          # type: ignore[attr-defined]
        _stdlib_time.ticks_cpu = ticks_cpu        # type: ignore[attr-defined]
        _stdlib_time.ticks_add = ticks_add        # type: ignore[attr-defined]
        _stdlib_time.ticks_diff = ticks_diff      # type: ignore[attr-defined]
        _stdlib_time.sleep_ms = sleep_ms          # type: ignore[attr-defined]
        _stdlib_time.sleep_us = sleep_us          # type: ignore[attr-defined]


def _casio_aware_open(file, mode="r", *args, **kwargs):
    """Allows Casio root paths like '/spot.obj' or '\\\\fls0\\data.txt' to resolve
    automatically to the user's script folder in Windows IDLE."""
    try:
        return _orig_open(file, mode, *args, **kwargs)
    except OSError:
        if isinstance(file, str):
            norm = file.replace("\\", "/")
            low = norm.lower()
            rel = None
            if low.startswith("//fls0/"):
                rel = norm[7:]
            elif low.startswith("/fls0/"):
                rel = norm[6:]
            elif norm.startswith("/") and not norm.startswith("//"):
                rel = norm.lstrip("/")
            if rel:
                bases = []
                if sys.argv and sys.argv[0]:
                    bases.append(os.path.dirname(os.path.abspath(sys.argv[0])))
                bases.append(os.getcwd())
                for b in bases:
                    cand = os.path.join(b, rel)
                    if os.path.exists(cand) or any(ch in mode for ch in ("w", "a", "x", "+")):
                        return _orig_open(cand, mode, *args, **kwargs)
        raise


_patch_stdlib_time()
if builtins.open is _orig_open:
    builtins.open = _casio_aware_open  # type: ignore[assignment]


# ============================================================================
# 2. EXACT CASIO FX-CG50 `gint` CONSTANTS (FROM PythonExtra modgint.c)
# ============================================================================
DWIDTH = 396
DHEIGHT = 224
_FB_BYTES = DWIDTH * DHEIGHT * 4

DTEXT_LEFT = 0
DTEXT_CENTER = 1
DTEXT_RIGHT = 2
DTEXT_TOP = 0
DTEXT_MIDDLE = 1
DTEXT_BOTTOM = 2

C_WHITE = 0xFFFF
C_LIGHT = 0xAD55
C_DARK = 0x528A
C_BLACK = 0x0000
C_RED = 0xF800
C_GREEN = 0x07E0
C_BLUE = 0x001F
C_NONE = -1
C_INVERT = -2

# Monochrome compatibility constants
C_LIGHTEN = -3
C_DARKEN = -4
DGRAY_ON = 0
DGRAY_OFF = 1
DGRAY_PUSH_ON = 2
DGRAY_PUSH_OFF = 3
DGRAY_POP = 4

# Image formats & flags
IMAGE_RGB565 = 0
IMAGE_RGB565A = 1
IMAGE_DEPRECATED_P8 = 2
IMAGE_P4_RGB565A = 3
IMAGE_P8_RGB565 = 4
IMAGE_P8_RGB565A = 5
IMAGE_P4_RGB565 = 6

IMAGE_MONO = 0
IMAGE_MONO_ALPHA = 1
IMAGE_GRAY = 2
IMAGE_GRAY_ALPHA = 3

IMAGE_FLAGS_DATA_RO = 1
IMAGE_FLAGS_PALETTE_RO = 2
IMAGE_FLAGS_DATA_ALLOC = 4
IMAGE_FLAGS_PALETTE_ALLOC = 8

# Key codes (<gint/keycodes.h>)
KEY_F1 = 0x91
KEY_F2 = 0x92
KEY_F3 = 0x93
KEY_F4 = 0x94
KEY_F5 = 0x95
KEY_F6 = 0x96

KEY_SHIFT = 0x81
KEY_OPTN = 0x82
KEY_VARS = 0x83
KEY_MENU = 0x84
KEY_LEFT = 0x85
KEY_UP = 0x86

KEY_ALPHA = 0x71
KEY_SQUARE = 0x72
KEY_POWER = 0x73
KEY_EXIT = 0x74
KEY_DOWN = 0x75
KEY_RIGHT = 0x76

KEY_XOT = 0x61
KEY_LOG = 0x62
KEY_LN = 0x63
KEY_SIN = 0x64
KEY_COS = 0x65
KEY_TAN = 0x66

KEY_FRAC = 0x51
KEY_FD = 0x52
KEY_LEFTP = 0x53
KEY_RIGHTP = 0x54
KEY_COMMA = 0x55
KEY_ARROW = 0x56

KEY_7 = 0x41
KEY_8 = 0x42
KEY_9 = 0x43
KEY_DEL = 0x44

KEY_4 = 0x31
KEY_5 = 0x32
KEY_6 = 0x33
KEY_MUL = 0x34
KEY_DIV = 0x35

KEY_1 = 0x21
KEY_2 = 0x22
KEY_3 = 0x23
KEY_ADD = 0x24
KEY_SUB = 0x25

KEY_0 = 0x11
KEY_DOT = 0x12
KEY_EXP = 0x13
KEY_NEG = 0x14
KEY_EXE = 0x15

KEY_ACON = 0x07
KEY_HELP = 0x20
KEY_LIGHT = 0x10

# CP-400 / fx-CG100 extra codes
KEY_KBD = 0xA1
KEY_X = 0xA2
KEY_Y = 0xA3
KEY_Z = 0xA4
KEY_EQUALS = 0xA5
KEY_CLEAR = 0xA6

KEY_ON = KEY_ACON
KEY_HOME = 0xB0
KEY_PREVTAB = 0xB1
KEY_NEXTTAB = 0xB2
KEY_PAGEUP = 0xB3
KEY_PAGEDOWN = 0xB4
KEY_SETTINGS = 0xB5
KEY_BACK = KEY_EXIT
KEY_OK = KEY_EXE
KEY_CATALOG = 0xB6
KEY_TOOLS = 0xB7
KEY_FORMAT = 0xB8
KEY_SQRT = 0xB9
KEY_EXPFUN = 0xBA

# Aliases
KEY_X2 = KEY_SQUARE
KEY_CARET = KEY_POWER
KEY_SWITCH = KEY_FD
KEY_LEFTPAR = KEY_LEFTP
KEY_RIGHTPAR = KEY_RIGHTP
KEY_STORE = KEY_ARROW
KEY_TIMES = KEY_MUL
KEY_PLUS = KEY_ADD
KEY_MINUS = KEY_SUB

# Event types & getkey options (<gint/keyboard.h>)
KEYEV_NONE = 0
KEYEV_DOWN = 1
KEYEV_UP = 2
KEYEV_HOLD = 3
KEYEV_TOUCH_DOWN = 4
KEYEV_TOUCH_DRAG = 5
KEYEV_TOUCH_UP = 6

GETKEY_NONE = 0x00
GETKEY_MOD_SHIFT = 0x01
GETKEY_MOD_ALPHA = 0x02
GETKEY_BACKLIGHT = 0x04
GETKEY_MENU = 0x08
GETKEY_REP_ARROWS = 0x10
GETKEY_REP_ALL = 0x20
GETKEY_REP_PROFILE = 0x40
GETKEY_FEATURES = 0x80
GETKEY_DEFAULT = 0xDF

KeyEvent = collections.namedtuple(
    "key_event", ("time", "mod", "shift", "alpha", "type", "key", "x", "y")
)


def C_RGB(r: int, g: int, b: int) -> int:
    """Create 16-bit RGB565 color from 5-bit channels r, g, b in 0..31."""
    return ((int(r) & 31) << 11) | ((int(g) & 31) << 6) | (int(b) & 31)


_LUT_CREF = [0] * 65536
_LUT_BGRA = [0] * 65536
for _c in range(65536):
    _r8 = (((_c >> 11) & 0x1F) * 255 + 15) // 31
    _g8 = (((_c >> 5) & 0x3F) * 255 + 31) // 63
    _b8 = ((_c & 0x1F) * 255 + 15) // 31
    _LUT_CREF[_c] = (_b8 << 16) | (_g8 << 8) | _r8
    _LUT_BGRA[_c] = 0xFF000000 | (_r8 << 16) | (_g8 << 8) | _b8


def _bgra_to_rgb565(bgra: int) -> int:
    r8 = (bgra >> 16) & 0xFF
    g8 = (bgra >> 8) & 0xFF
    b8 = bgra & 0xFF
    return ((r8 >> 3) << 11) | ((g8 >> 2) << 5) | (b8 >> 3)


# ============================================================================
# 3. `gint.image` AND `gint.font` OBJECTS
# ============================================================================
class font:
    """Compatible wrapper for gint.font objects."""

    def __init__(self, *args, **kwargs):
        self.line_height = 11
        self.char_width = 6


class image:
    """Compatible wrapper for gint.image (RGB565 / P8 / P4, with/without alpha)."""

    def __init__(
        self,
        format: int,
        color_count: int,
        width: int,
        height: int,
        stride: int,
        data,
        palette=None,
    ):
        if width <= 0 or height <= 0:
            raise ValueError("image width/height must be >0")
        self.format = int(format)
        self.color_count = int(color_count)
        self.width = int(width)
        self.height = int(height)
        self.stride = int(stride)
        self.data = memoryview(data)
        self.palette = memoryview(palette) if palette is not None else None
        self._decoded = self._decode_to_bgra()

    def _decode_to_bgra(self) -> list[int | None]:
        w, h, st, fmt = self.width, self.height, self.stride, self.format
        raw = self.data.cast("B")
        out: list[int | None] = [None] * (w * h)

        pal16: list[int] = []
        if self.palette is not None:
            pb = self.palette.cast("B")
            for i in range(0, len(pb) - 1, 2):
                pal16.append((pb[i] << 8) | pb[i + 1])

        if fmt in (IMAGE_RGB565, IMAGE_RGB565A):
            has_a = fmt == IMAGE_RGB565A
            for y in range(h):
                row_off = y * st
                dst_off = y * w
                for x in range(w):
                    idx = row_off + x * 2
                    if idx + 1 < len(raw):
                        c16 = (raw[idx] << 8) | raw[idx + 1]
                        if has_a and c16 == 0x0001:
                            out[dst_off + x] = None
                        else:
                            out[dst_off + x] = _LUT_BGRA[c16]
        elif fmt in (IMAGE_P8_RGB565, IMAGE_P8_RGB565A):
            has_a = fmt == IMAGE_P8_RGB565A
            for y in range(h):
                row_off = y * st
                dst_off = y * w
                for x in range(w):
                    idx = row_off + x
                    if idx < len(raw):
                        b = raw[idx]
                        if has_a and b == 0x80:
                            out[dst_off + x] = None
                        else:
                            pi = (
                                ((b - 128) if b >= 128 else (b + 128))
                                if len(pal16) > 128
                                else (b % max(1, len(pal16)))
                            )
                            if 0 <= pi < len(pal16):
                                out[dst_off + x] = _LUT_BGRA[pal16[pi]]
        elif fmt in (IMAGE_P4_RGB565, IMAGE_P4_RGB565A):
            has_a = fmt == IMAGE_P4_RGB565A
            for y in range(h):
                row_off = y * st
                dst_off = y * w
                for x in range(w):
                    b_idx = row_off + (x >> 1)
                    if b_idx < len(raw):
                        nib = (raw[b_idx] >> 4) if (x & 1) == 0 else (raw[b_idx] & 0x0F)
                        if has_a and nib == 0:
                            out[dst_off + x] = None
                        elif nib < len(pal16):
                            out[dst_off + x] = _LUT_BGRA[pal16[nib]]
        return out


def image_rgb565(width: int, height: int, data) -> image:
    return image(IMAGE_RGB565, 0, width, height, width * 2, data, None)


def image_rgb565a(width: int, height: int, data) -> image:
    return image(IMAGE_RGB565A, 0, width, height, width * 2, data, None)


def image_p8_rgb565(width: int, height: int, data, palette) -> image:
    return image(IMAGE_P8_RGB565, len(palette) // 2, width, height, width, data, palette)


def image_p8_rgb565a(width: int, height: int, data, palette) -> image:
    return image(IMAGE_P8_RGB565A, len(palette) // 2, width, height, width, data, palette)


def image_p4_rgb565(width: int, height: int, data, palette) -> image:
    return image(IMAGE_P4_RGB565, 16, width, height, (width + 1) // 2, data, palette)


def image_p4_rgb565a(width: int, height: int, data, palette) -> image:
    return image(IMAGE_P4_RGB565A, 16, width, height, (width + 1) // 2, data, palette)


# ============================================================================
# 4. WIN32 DOUBLE-BUFFERED DEDICATED-THREAD GDI32 BACKEND
# ============================================================================
_VK_TO_CASIO: dict[int, int] = {
    0x70: KEY_F1, 0x71: KEY_F2, 0x72: KEY_F3,
    0x73: KEY_F4, 0x74: KEY_F5, 0x75: KEY_F6,
    0x25: KEY_LEFT, 0x26: KEY_UP, 0x27: KEY_RIGHT, 0x28: KEY_DOWN,
    0x0D: KEY_EXE, 0x1B: KEY_EXIT, 0x08: KEY_DEL, 0x2E: KEY_DEL,
    0x10: KEY_SHIFT, 0xA0: KEY_SHIFT, 0xA1: KEY_SHIFT,
    0x11: KEY_ALPHA, 0xA2: KEY_ALPHA, 0xA3: KEY_ALPHA, 0x14: KEY_ALPHA,
    0x09: KEY_OPTN, 0x24: KEY_MENU, 0x21: KEY_VARS, 0x22: KEY_ACON,
    0x30: KEY_0, 0x31: KEY_1, 0x32: KEY_2, 0x33: KEY_3, 0x34: KEY_4,
    0x35: KEY_5, 0x36: KEY_6, 0x37: KEY_7, 0x38: KEY_8, 0x39: KEY_9,
    0x60: KEY_0, 0x61: KEY_1, 0x62: KEY_2, 0x63: KEY_3, 0x64: KEY_4,
    0x65: KEY_5, 0x66: KEY_6, 0x67: KEY_7, 0x68: KEY_8, 0x69: KEY_9,
    0x6B: KEY_ADD, 0xBB: KEY_ADD,
    0x6D: KEY_SUB, 0xBD: KEY_SUB,
    0x6A: KEY_MUL,
    0x6F: KEY_DIV, 0xBF: KEY_DIV,
    0x6E: KEY_DOT, 0xBE: KEY_DOT,
    0xBC: KEY_COMMA,
    0xDB: KEY_LEFTP, 0xDD: KEY_RIGHTP,
    0xC0: KEY_NEG,
    # QWERTY A..Z -> Casio ALPHA physical keys
    0x41: KEY_XOT, 0x42: KEY_LOG, 0x43: KEY_LN, 0x44: KEY_SIN,
    0x45: KEY_COS, 0x46: KEY_TAN, 0x47: KEY_FRAC, 0x48: KEY_FD,
    0x49: KEY_LEFTP, 0x4A: KEY_RIGHTP, 0x4B: KEY_COMMA, 0x4C: KEY_ARROW,
    0x4D: KEY_7, 0x4E: KEY_8, 0x4F: KEY_9,
    0x50: KEY_4, 0x51: KEY_5, 0x52: KEY_6, 0x53: KEY_MUL, 0x54: KEY_DIV,
    0x55: KEY_1, 0x56: KEY_2, 0x57: KEY_3, 0x58: KEY_ADD, 0x59: KEY_SUB,
    0x5A: KEY_0,
}

if sys.platform == "win32":
    from ctypes import wintypes

    user32 = ctypes.windll.user32
    gdi32 = ctypes.windll.gdi32
    kernel32 = ctypes.windll.kernel32

    WPARAM = ctypes.c_size_t
    LPARAM = ctypes.c_ssize_t
    LRESULT = ctypes.c_ssize_t
    WNDPROC = ctypes.WINFUNCTYPE(LRESULT, wintypes.HWND, wintypes.UINT, WPARAM, LPARAM)

    class WNDCLASSW(ctypes.Structure):
        _fields_ = [
            ("style", wintypes.UINT),
            ("lpfnWndProc", WNDPROC),
            ("cbClsExtra", ctypes.c_int),
            ("cbWndExtra", ctypes.c_int),
            ("hInstance", wintypes.HINSTANCE),
            ("hIcon", wintypes.HICON),
            ("hCursor", wintypes.HANDLE),
            ("hbrBackground", wintypes.HBRUSH),
            ("lpszMenuName", wintypes.LPCWSTR),
            ("lpszClassName", wintypes.LPCWSTR),
        ]

    class BITMAPINFOHEADER(ctypes.Structure):
        _fields_ = [
            ("biSize", wintypes.DWORD),
            ("biWidth", wintypes.LONG),
            ("biHeight", wintypes.LONG),
            ("biPlanes", wintypes.WORD),
            ("biBitCount", wintypes.WORD),
            ("biCompression", wintypes.DWORD),
            ("biSizeImage", wintypes.DWORD),
            ("biXPelsPerMeter", wintypes.LONG),
            ("biYPelsPerMeter", wintypes.LONG),
            ("biClrUsed", wintypes.DWORD),
            ("biClrImportant", wintypes.DWORD),
        ]

    class BITMAPINFO(ctypes.Structure):
        _fields_ = [("bmiHeader", BITMAPINFOHEADER), ("bmiColors", wintypes.DWORD * 3)]

    class PAINTSTRUCT(ctypes.Structure):
        _fields_ = [
            ("hdc", wintypes.HDC),
            ("fErase", wintypes.BOOL),
            ("rcPaint", wintypes.RECT),
            ("fRestore", wintypes.BOOL),
            ("fIncUpdate", wintypes.BOOL),
            ("rgbReserved", wintypes.BYTE * 32),
        ]

    kernel32.GetModuleHandleW.argtypes = [wintypes.LPCWSTR]
    kernel32.GetModuleHandleW.restype = wintypes.HMODULE

    user32.DefWindowProcW.argtypes = [wintypes.HWND, wintypes.UINT, WPARAM, LPARAM]
    user32.DefWindowProcW.restype = LRESULT
    user32.RegisterClassW.argtypes = [ctypes.POINTER(WNDCLASSW)]
    user32.RegisterClassW.restype = wintypes.ATOM
    user32.UnregisterClassW.argtypes = [wintypes.LPCWSTR, wintypes.HINSTANCE]
    user32.CreateWindowExW.argtypes = [
        wintypes.DWORD, wintypes.LPCWSTR, wintypes.LPCWSTR, wintypes.DWORD,
        ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int,
        wintypes.HWND, wintypes.HMENU, wintypes.HINSTANCE, wintypes.LPVOID,
    ]
    user32.CreateWindowExW.restype = wintypes.HWND
    user32.DestroyWindow.argtypes = [wintypes.HWND]
    user32.PostQuitMessage.argtypes = [ctypes.c_int]
    user32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, WPARAM, LPARAM]
    user32.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
    user32.UpdateWindow.argtypes = [wintypes.HWND]
    user32.SetForegroundWindow.argtypes = [wintypes.HWND]
    user32.SetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPCWSTR]
    user32.AdjustWindowRect.argtypes = [ctypes.POINTER(wintypes.RECT), wintypes.DWORD, wintypes.BOOL]
    user32.SetWindowPos.argtypes = [
        wintypes.HWND, wintypes.HWND, ctypes.c_int, ctypes.c_int,
        ctypes.c_int, ctypes.c_int, wintypes.UINT,
    ]
    user32.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
    user32.InvalidateRect.argtypes = [wintypes.HWND, ctypes.c_void_p, wintypes.BOOL]
    user32.GetMessageW.argtypes = [ctypes.POINTER(wintypes.MSG), wintypes.HWND, wintypes.UINT, wintypes.UINT]
    user32.GetMessageW.restype = wintypes.BOOL
    user32.TranslateMessage.argtypes = [ctypes.POINTER(wintypes.MSG)]
    user32.DispatchMessageW.argtypes = [ctypes.POINTER(wintypes.MSG)]
    user32.DispatchMessageW.restype = LRESULT
    user32.BeginPaint.argtypes = [wintypes.HWND, ctypes.POINTER(PAINTSTRUCT)]
    user32.BeginPaint.restype = wintypes.HDC
    user32.EndPaint.argtypes = [wintypes.HWND, ctypes.POINTER(PAINTSTRUCT)]
    user32.FillRect.argtypes = [wintypes.HDC, ctypes.POINTER(wintypes.RECT), wintypes.HBRUSH]
    user32.LoadCursorW.argtypes = [wintypes.HINSTANCE, ctypes.c_void_p]
    user32.LoadCursorW.restype = wintypes.HANDLE

    gdi32.CreateCompatibleDC.argtypes = [wintypes.HDC]
    gdi32.CreateCompatibleDC.restype = wintypes.HDC
    gdi32.DeleteDC.argtypes = [wintypes.HDC]
    gdi32.CreateDIBSection.argtypes = [
        wintypes.HDC, ctypes.POINTER(BITMAPINFO), wintypes.UINT,
        ctypes.POINTER(ctypes.c_void_p), wintypes.HANDLE, wintypes.DWORD,
    ]
    gdi32.CreateDIBSection.restype = wintypes.HBITMAP
    gdi32.SelectObject.argtypes = [wintypes.HDC, wintypes.HGDIOBJ]
    gdi32.SelectObject.restype = wintypes.HGDIOBJ
    gdi32.DeleteObject.argtypes = [wintypes.HGDIOBJ]
    gdi32.GetStockObject.argtypes = [ctypes.c_int]
    gdi32.GetStockObject.restype = wintypes.HGDIOBJ
    gdi32.SetDCBrushColor.argtypes = [wintypes.HDC, wintypes.COLORREF]
    gdi32.SetDCPenColor.argtypes = [wintypes.HDC, wintypes.COLORREF]
    gdi32.SetBkMode.argtypes = [wintypes.HDC, ctypes.c_int]
    gdi32.SetBkColor.argtypes = [wintypes.HDC, wintypes.COLORREF]
    gdi32.SetTextColor.argtypes = [wintypes.HDC, wintypes.COLORREF]
    gdi32.SetPolyFillMode.argtypes = [wintypes.HDC, ctypes.c_int]
    gdi32.MoveToEx.argtypes = [wintypes.HDC, ctypes.c_int, ctypes.c_int, ctypes.c_void_p]
    gdi32.LineTo.argtypes = [wintypes.HDC, ctypes.c_int, ctypes.c_int]
    gdi32.Polygon.argtypes = [wintypes.HDC, ctypes.POINTER(wintypes.POINT), ctypes.c_int]
    gdi32.Ellipse.argtypes = [wintypes.HDC, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    gdi32.PatBlt.argtypes = [wintypes.HDC, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wintypes.DWORD]
    gdi32.SetStretchBltMode.argtypes = [wintypes.HDC, ctypes.c_int]
    gdi32.StretchBlt.argtypes = [
        wintypes.HDC, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int,
        wintypes.HDC, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wintypes.DWORD,
    ]
    gdi32.CreateFontW.argtypes = [
        ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int,
        wintypes.DWORD, wintypes.DWORD, wintypes.DWORD, wintypes.DWORD,
        wintypes.DWORD, wintypes.DWORD, wintypes.DWORD, wintypes.DWORD, wintypes.LPCWSTR,
    ]
    gdi32.CreateFontW.restype = wintypes.HFONT
    gdi32.TextOutW.argtypes = [wintypes.HDC, ctypes.c_int, ctypes.c_int, wintypes.LPCWSTR, ctypes.c_int]
    gdi32.GetTextExtentPoint32W.argtypes = [
        wintypes.HDC, wintypes.LPCWSTR, ctypes.c_int, ctypes.POINTER(wintypes.SIZE)
    ]

    try:
        user32.SetProcessDPIAware()
    except Exception:
        pass


def _create_dib_dc():
    """Creates a 396x224 top-down 32-bit BGRA memory DC + raw uint32 pointer."""
    dc = gdi32.CreateCompatibleDC(None)
    bmi = BITMAPINFO()
    bmi.bmiHeader.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bmi.bmiHeader.biWidth = DWIDTH
    bmi.bmiHeader.biHeight = -DHEIGHT  # Top-down
    bmi.bmiHeader.biPlanes = 1
    bmi.bmiHeader.biBitCount = 32
    bmi.bmiHeader.biCompression = 0
    bits = ctypes.c_void_p()
    hbmp = gdi32.CreateDIBSection(dc, ctypes.byref(bmi), 0, ctypes.byref(bits), None, 0)
    old_bmp = gdi32.SelectObject(dc, hbmp)
    arr = ctypes.cast(bits, ctypes.POINTER(ctypes.c_uint32 * (DWIDTH * DHEIGHT))).contents
    return dc, hbmp, old_bmp, bits, arr


class _Win32Backend:
    WM_DESTROY = 0x0002
    WM_KILLFOCUS = 0x0008
    WM_PAINT = 0x000F
    WM_CLOSE = 0x0010
    WM_ERASEBKGND = 0x0014
    WM_KEYDOWN = 0x0100
    WM_KEYUP = 0x0101
    WM_SYSKEYDOWN = 0x0104
    WM_SYSKEYUP = 0x0105
    WM_USER_SHUTDOWN = 0x0401

    WS_OVERLAPPEDWINDOW = 0x00CF0000
    SWP_NOMOVE = 0x0002
    SWP_NOZORDER = 0x0004
    SRCCOPY = 0x00CC0020
    DSTINVERT = 0x00550009
    COLORONCOLOR = 3
    TRANSPARENT = 1
    OPAQUE = 2
    NULL_BRUSH = 5
    NULL_PEN = 8
    DC_BRUSH = 18
    DC_PEN = 19

    def __init__(self, scale: int = 2) -> None:
        self.scale = scale
        self.closed = False
        self.shot_idx = 1

        self._key_cv = threading.Condition()
        self._present_lock = threading.Lock()
        self.down_keys: set[int] = set()
        self.pressed_flips: set[int] = set()
        self.released_flips: set[int] = set()
        self.event_queue: collections.deque[KeyEvent] = collections.deque(maxlen=256)

        # Back buffer: exclusively owned by main Python thread (100% lock-free drawing!)
        (
            self.mem_dc,
            self.hbmp,
            self._old_bmp,
            self._back_ptr,
            self.pixels,
        ) = _create_dib_dc()

        # Front buffer: shared only during 0.03ms memmove in dupdate() & WM_PAINT
        (
            self.front_dc,
            self.front_hbmp,
            self._front_old_bmp,
            self._front_ptr,
            self.front_pixels,
        ) = _create_dib_dc()

        self.dc_brush = gdi32.GetStockObject(self.DC_BRUSH)
        self.null_brush = gdi32.GetStockObject(self.NULL_BRUSH)
        self.dc_pen = gdi32.GetStockObject(self.DC_PEN)
        self.null_pen = gdi32.GetStockObject(self.NULL_PEN)

        self.hfont = gdi32.CreateFontW(
            -11, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 3, 0x31, "Consolas"
        )
        if not self.hfont:
            self.hfont = gdi32.GetStockObject(16)
        self._old_font = gdi32.SelectObject(self.mem_dc, self.hfont)
        gdi32.SetPolyFillMode(self.mem_dc, 2)

        self._pt_cap = 64
        self._pt_buf = (wintypes.POINT * self._pt_cap)()
        self._zbuf = [0] * (DWIDTH * DHEIGHT)

        self.clear(C_WHITE)
        ctypes.memmove(self._front_ptr, self._back_ptr, _FB_BYTES)

        self.hwnd = None
        self._ready_ev = threading.Event()
        self._ui_thread = threading.Thread(target=self._ui_loop, name="GintWin32UI", daemon=True)
        self._ui_thread.start()
        self._ready_ev.wait(timeout=3.0)

    def _title_str(self) -> str:
        return f"Casio fx-CG50 (gint {DWIDTH}x{DHEIGHT} @{self.scale}x)  [F9:Zoom  F12:BMP  Esc:EXIT]"

    def _ui_loop(self) -> None:
        self.hinst = kernel32.GetModuleHandleW(None)
        self.cls_name = f"CasioGintIDLE_{os.getpid()}_{id(self)}"
        self._wndproc_ref = WNDPROC(self._wndproc)

        wc = WNDCLASSW()
        wc.style = 0x0003
        wc.lpfnWndProc = self._wndproc_ref
        wc.cbClsExtra = 0
        wc.cbWndExtra = 0
        wc.hInstance = self.hinst
        wc.hIcon = None
        wc.hCursor = user32.LoadCursorW(None, 32512)
        wc.hbrBackground = gdi32.GetStockObject(4)
        wc.lpszMenuName = None
        wc.lpszClassName = self.cls_name
        user32.RegisterClassW(ctypes.byref(wc))

        rc = wintypes.RECT(0, 0, DWIDTH * self.scale, DHEIGHT * self.scale)
        user32.AdjustWindowRect(ctypes.byref(rc), self.WS_OVERLAPPEDWINDOW, False)
        self.hwnd = user32.CreateWindowExW(
            0, self.cls_name, self._title_str(), self.WS_OVERLAPPEDWINDOW,
            120, 100, rc.right - rc.left, rc.bottom - rc.top,
            None, None, self.hinst, None,
        )
        user32.ShowWindow(self.hwnd, 5)
        user32.UpdateWindow(self.hwnd)
        user32.SetForegroundWindow(self.hwnd)
        self._ready_ev.set()

        msg = wintypes.MSG()
        while user32.GetMessageW(ctypes.byref(msg), None, 0, 0) > 0:
            user32.TranslateMessage(ctypes.byref(msg))
            user32.DispatchMessageW(ctypes.byref(msg))

        user32.UnregisterClassW(self.cls_name, self.hinst)

    def _set_scale_ui(self, new_scale: int) -> None:
        self.scale = new_scale
        rc = wintypes.RECT(0, 0, DWIDTH * self.scale, DHEIGHT * self.scale)
        user32.AdjustWindowRect(ctypes.byref(rc), self.WS_OVERLAPPEDWINDOW, False)
        user32.SetWindowPos(
            self.hwnd, None, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
            self.SWP_NOMOVE | self.SWP_NOZORDER,
        )
        user32.SetWindowTextW(self.hwnd, self._title_str())
        user32.InvalidateRect(self.hwnd, None, False)

    def _save_bmp_ui(self) -> None:
        fname = f"gint_shot_{self.shot_idx:04d}.bmp"
        self.shot_idx += 1
        row_bytes = DWIDTH * 3
        pad = (4 - (row_bytes % 4)) % 4
        img_sz = (row_bytes + pad) * DHEIGHT
        hdr = struct.pack("<2sIHHI", b"BM", 54 + img_sz, 0, 0, 54)
        dib = struct.pack("<IiiHHIIiiII", 40, DWIDTH, DHEIGHT, 1, 24, 0, img_sz, 2835, 2835, 0, 0)
        rows = bytearray()
        with self._present_lock:
            px = list(self.front_pixels)
        for y in range(DHEIGHT - 1, -1, -1):
            base = y * DWIDTH
            for x in range(DWIDTH):
                c = px[base + x]
                rows.append(c & 0xFF)
                rows.append((c >> 8) & 0xFF)
                rows.append((c >> 16) & 0xFF)
            if pad:
                rows.extend(b"\x00" * pad)
        with _orig_open(fname, "wb") as f:
            f.write(hdr)
            f.write(dib)
            f.write(rows)
        if self.hwnd:
            user32.SetWindowTextW(self.hwnd, f"Saved {fname}!  |  " + self._title_str())

    def _wndproc(self, hwnd, msg, wparam, lparam) -> int:
        if msg == self.WM_ERASEBKGND:
            return 1
        if msg == self.WM_PAINT:
            ps = PAINTSTRUCT()
            hdc = user32.BeginPaint(hwnd, ctypes.byref(ps))
            rc = wintypes.RECT()
            user32.GetClientRect(hwnd, ctypes.byref(rc))
            cw = max(1, rc.right - rc.left)
            ch = max(1, rc.bottom - rc.top)
            gdi32.SetStretchBltMode(hdc, self.COLORONCOLOR)
            with self._present_lock:
                gdi32.StretchBlt(
                    hdc, 0, 0, cw, ch, self.front_dc, 0, 0, DWIDTH, DHEIGHT, self.SRCCOPY
                )
            user32.EndPaint(hwnd, ctypes.byref(ps))
            return 0
        if msg in (self.WM_KEYDOWN, self.WM_SYSKEYDOWN):
            vk = int(wparam) & 0xFF
            if vk == 0x78:  # F9 -> Zoom
                if not (lparam & (1 << 30)):
                    self._set_scale_ui((self.scale % 4) + 1)
                return 0
            if vk == 0x7B:  # F12 -> BMP screenshot
                if not (lparam & (1 << 30)):
                    self._save_bmp_ui()
                return 0
            ckey = _VK_TO_CASIO.get(vk)
            if ckey is not None:
                was_down = (lparam & (1 << 30)) != 0
                with self._key_cv:
                    if not was_down and ckey not in self.down_keys:
                        self.down_keys.add(ckey)
                        self.pressed_flips.add(ckey)
                        ev_t = KEYEV_DOWN
                    else:
                        ev_t = KEYEV_HOLD
                    sh = KEY_SHIFT in self.down_keys
                    al = KEY_ALPHA in self.down_keys
                    self.event_queue.append(KeyEvent(ticks_ms(), sh or al, sh, al, ev_t, ckey, 0, 0))
                    self._key_cv.notify_all()
                return 0
        if msg in (self.WM_KEYUP, self.WM_SYSKEYUP):
            vk = int(wparam) & 0xFF
            ckey = _VK_TO_CASIO.get(vk)
            if ckey is not None:
                with self._key_cv:
                    self.down_keys.discard(ckey)
                    self.released_flips.add(ckey)
                    sh = KEY_SHIFT in self.down_keys
                    al = KEY_ALPHA in self.down_keys
                    self.event_queue.append(
                        KeyEvent(ticks_ms(), sh or al, sh, al, KEYEV_UP, ckey, 0, 0)
                    )
                    self._key_cv.notify_all()
                return 0
        if msg == self.WM_KILLFOCUS:
            with self._key_cv:
                self.down_keys.clear()
            return 0
        if msg in (self.WM_CLOSE, self.WM_USER_SHUTDOWN):
            with self._key_cv:
                self.closed = True
                self.down_keys.add(KEY_EXIT)
                self.pressed_flips.add(KEY_EXIT)
                self.event_queue.append(
                    KeyEvent(ticks_ms(), False, False, False, KEYEV_DOWN, KEY_EXIT, 0, 0)
                )
                self._key_cv.notify_all()
            user32.DestroyWindow(hwnd)
            return 0
        if msg == self.WM_DESTROY:
            self.closed = True
            self.hwnd = None
            user32.PostQuitMessage(0)
            return 0
        return user32.DefWindowProcW(hwnd, msg, wparam, lparam)

    def pump(self) -> None:
        pass  # Dedicated UI thread pumps messages continuously!

    def present(self) -> None:
        if self.closed:
            return
        with self._present_lock:
            ctypes.memmove(self._front_ptr, self._back_ptr, _FB_BYTES)
        if self.hwnd:
            user32.InvalidateRect(self.hwnd, None, False)

    def destroy(self) -> None:
        if self.hwnd:
            user32.PostMessageW(self.hwnd, self.WM_USER_SHUTDOWN, 0, 0)
            self._ui_thread.join(timeout=1.0)
        if getattr(self, "mem_dc", None):
            gdi32.SelectObject(self.mem_dc, self._old_bmp)
            gdi32.SelectObject(self.mem_dc, self._old_font)
            gdi32.DeleteObject(self.hbmp)
            if self.hfont:
                gdi32.DeleteObject(self.hfont)
            gdi32.DeleteDC(self.mem_dc)
            self.mem_dc = None
        if getattr(self, "front_dc", None):
            gdi32.SelectObject(self.front_dc, self._front_old_bmp)
            gdi32.DeleteObject(self.front_hbmp)
            gdi32.DeleteDC(self.front_dc)
            self.front_dc = None

    # --- Lock-Free C/GDI Drawing Primitives on Back Buffer ---
    def clear(self, color: int) -> None:
        if color == C_NONE:
            return
        if color == C_INVERT:
            gdi32.PatBlt(self.mem_dc, 0, 0, DWIDTH, DHEIGHT, self.DSTINVERT)
            return
        rc = wintypes.RECT(0, 0, DWIDTH, DHEIGHT)
        gdi32.SetDCBrushColor(self.mem_dc, _LUT_CREF[color & 0xFFFF])
        user32.FillRect(self.mem_dc, ctypes.byref(rc), self.dc_brush)

    def rect(self, x1: int, y1: int, x2: int, y2: int, color: int) -> None:
        if color == C_NONE:
            return
        if x1 > x2:
            x1, x2 = x2, x1
        if y1 > y2:
            y1, y2 = y2, y1
        if x2 < 0 or y2 < 0 or x1 >= DWIDTH or y1 >= DHEIGHT:
            return
        if color == C_INVERT:
            gdi32.PatBlt(self.mem_dc, x1, y1, x2 - x1 + 1, y2 - y1 + 1, self.DSTINVERT)
            return
        rc = wintypes.RECT(x1, y1, x2 + 1, y2 + 1)
        gdi32.SetDCBrushColor(self.mem_dc, _LUT_CREF[color & 0xFFFF])
        user32.FillRect(self.mem_dc, ctypes.byref(rc), self.dc_brush)

    def pixel(self, x: int, y: int, color: int) -> None:
        if color == C_NONE or not (0 <= x < DWIDTH and 0 <= y < DHEIGHT):
            return
        idx = y * DWIDTH + x
        if color == C_INVERT:
            self.pixels[idx] ^= 0x00FFFFFF
        else:
            self.pixels[idx] = _LUT_BGRA[color & 0xFFFF]

    def getpixel(self, x: int, y: int) -> int:
        if not (0 <= x < DWIDTH and 0 <= y < DHEIGHT):
            return 0
        return _bgra_to_rgb565(self.pixels[y * DWIDTH + x])

    def line(self, x1: int, y1: int, x2: int, y2: int, color: int) -> None:
        if color == C_NONE:
            return
        if color == C_INVERT:
            self._line_invert(x1, y1, x2, y2)
            return
        cref = _LUT_CREF[color & 0xFFFF]
        gdi32.SelectObject(self.mem_dc, self.dc_pen)
        gdi32.SetDCPenColor(self.mem_dc, cref)
        gdi32.MoveToEx(self.mem_dc, x1, y1, None)
        gdi32.LineTo(self.mem_dc, x2, y2)
        if 0 <= x2 < DWIDTH and 0 <= y2 < DHEIGHT:
            self.pixels[y2 * DWIDTH + x2] = _LUT_BGRA[color & 0xFFFF]

    def _line_invert(self, x1: int, y1: int, x2: int, y2: int) -> None:
        dx = abs(x2 - x1)
        sx = 1 if x1 < x2 else -1
        dy = -abs(y2 - y1)
        sy = 1 if y1 < y2 else -1
        err = dx + dy
        px = self.pixels
        while True:
            if 0 <= x1 < DWIDTH and 0 <= y1 < DHEIGHT:
                px[y1 * DWIDTH + x1] ^= 0x00FFFFFF
            if x1 == x2 and y1 == y2:
                break
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x1 += sx
            if e2 <= dx:
                err += dx
                y1 += sy

    def ellipse(self, x1: int, y1: int, x2: int, y2: int, fill: int, border: int) -> None:
        if fill == C_NONE and border == C_NONE:
            return
        if x1 > x2:
            x1, x2 = x2, x1
        if y1 > y2:
            y1, y2 = y2, y1
        if fill >= 0:
            gdi32.SelectObject(self.mem_dc, self.dc_brush)
            gdi32.SetDCBrushColor(self.mem_dc, _LUT_CREF[fill & 0xFFFF])
        else:
            gdi32.SelectObject(self.mem_dc, self.null_brush)
        if border >= 0:
            gdi32.SelectObject(self.mem_dc, self.dc_pen)
            gdi32.SetDCPenColor(self.mem_dc, _LUT_CREF[border & 0xFFFF])
        elif fill >= 0:
            gdi32.SelectObject(self.mem_dc, self.dc_pen)
            gdi32.SetDCPenColor(self.mem_dc, _LUT_CREF[fill & 0xFFFF])
        else:
            gdi32.SelectObject(self.mem_dc, self.null_pen)
        gdi32.Ellipse(self.mem_dc, x1, y1, x2 + 1, y2 + 1)

    def poly(self, vertices, fill: int, border: int) -> None:
        n = len(vertices) // 2
        if n < 2 or (fill == C_NONE and border == C_NONE):
            return
        if n > self._pt_cap:
            self._pt_cap = max(n, self._pt_cap * 2)
            self._pt_buf = (wintypes.POINT * self._pt_cap)()
        pts = self._pt_buf
        for i in range(n):
            pts[i].x = int(vertices[2 * i])
            pts[i].y = int(vertices[2 * i + 1])

        if fill >= 0:
            gdi32.SelectObject(self.mem_dc, self.dc_brush)
            gdi32.SetDCBrushColor(self.mem_dc, _LUT_CREF[fill & 0xFFFF])
        else:
            gdi32.SelectObject(self.mem_dc, self.null_brush)

        if border >= 0:
            gdi32.SelectObject(self.mem_dc, self.dc_pen)
            gdi32.SetDCPenColor(self.mem_dc, _LUT_CREF[border & 0xFFFF])
        elif fill >= 0:
            gdi32.SelectObject(self.mem_dc, self.dc_pen)
            gdi32.SetDCPenColor(self.mem_dc, _LUT_CREF[fill & 0xFFFF])
        else:
            gdi32.SelectObject(self.mem_dc, self.null_pen)

        gdi32.Polygon(self.mem_dc, pts, n)

    def clear_zbuf(self) -> None:
        self._zbuf[:] = [0] * (DWIDTH * DHEIGHT)

    def tri_zbuf(
        self,
        x0: int, y0: int, z0: int,
        x1: int, y1: int, z1: int,
        x2: int, y2: int, z2: int,
        color: int,
        ymin: int = 0,
        ymax: int = DHEIGHT - 1,
    ) -> None:
        if color == C_NONE:
            return
        if y0 > y1:
            x0, x1 = x1, x0; y0, y1 = y1, y0; z0, z1 = z1, z0
        if y1 > y2:
            x1, x2 = x2, x1; y1, y2 = y2, y1; z1, z2 = z2, z1
        if y0 > y1:
            x0, x1 = x1, x0; y0, y1 = y1, y0; z0, z1 = z1, z0
        if y2 < ymin or y0 > ymax or y0 == y2:
            return
        col32 = _LUT_BGRA[color & 0xFFFF]
        th = y2 - y0
        dxl = ((x2 - x0) << 16) // th
        dzl = ((z2 - z0) << 15) // th
        zb = self._zbuf
        px = self.pixels
        for part in (0, 1):
            ys = y0 if part == 0 else y1
            ye = y1 if part == 0 else y2
            sh = ye - ys
            if sh <= 0:
                continue
            y_end = (y1 - 1) if (part == 0 and y1 < y2) else ye
            y_st = ymin if ys < ymin else ys
            if y_end > ymax:
                y_end = ymax
            if y_st > y_end:
                continue
            xs_b = x0 if part == 0 else x1
            zs_b = z0 if part == 0 else z1
            xe_t = x1 if part == 0 else x2
            ze_t = z1 if part == 0 else z2
            dxs = ((xe_t - xs_b) << 16) // sh
            dzs = ((ze_t - zs_b) << 15) // sh
            xl = (x0 << 16) + dxl * (y_st - y0)
            zl = (z0 << 15) + dzl * (y_st - y0)
            xs = (xs_b << 16) + dxs * (y_st - ys)
            zs = (zs_b << 15) + dzs * (y_st - ys)
            for y in range(y_st, y_end + 1):
                xa = xl >> 16
                xb = xs >> 16
                za = zl
                zb_v = zs
                xl += dxl; zl += dzl; xs += dxs; zs += dzs
                if xa > xb:
                    xa, xb = xb, xa; za, zb_v = zb_v, za
                if xb < 0 or xa >= DWIDTH:
                    continue
                sp = xb - xa
                dz = (zb_v - za) // sp if sp > 0 else 0
                cz = za
                if xa < 0:
                    cz += dz * (-xa)
                    xa = 0
                if xb >= DWIDTH:
                    xb = DWIDTH - 1
                row = y * DWIDTH
                for idx in range(row + xa, row + xb + 1):
                    iz = cz >> 15
                    if iz > zb[idx]:
                        zb[idx] = iz
                        px[idx] = col32
                    cz += dz

    def text_opt(
        self, x: int, y: int, fg: int, bg: int, halign: int, valign: int, s: str, size: int
    ) -> None:
        if fg == C_NONE or not s:
            return
        txt = s[:size] if size >= 0 else s
        if not txt:
            return
        n = len(txt)
        sz = wintypes.SIZE()
        gdi32.GetTextExtentPoint32W(self.mem_dc, txt, n, ctypes.byref(sz))
        if halign == DTEXT_CENTER:
            x -= sz.cx // 2
        elif halign == DTEXT_RIGHT:
            x -= sz.cx
        if valign == DTEXT_MIDDLE:
            y -= sz.cy // 2
        elif valign == DTEXT_BOTTOM:
            y -= sz.cy

        if bg >= 0:
            gdi32.SetBkMode(self.mem_dc, self.OPAQUE)
            gdi32.SetBkColor(self.mem_dc, _LUT_CREF[bg & 0xFFFF])
        else:
            gdi32.SetBkMode(self.mem_dc, self.TRANSPARENT)

        fg_c = 0x000000 if fg == C_INVERT else _LUT_CREF[fg & 0xFFFF]
        gdi32.SetTextColor(self.mem_dc, fg_c)
        gdi32.TextOutW(self.mem_dc, x, y, txt, n)

    def subimage(
        self, x: int, y: int, img: image, left: int, top: int, w: int, h: int
    ) -> None:
        src = img._decoded
        iw, ih = img.width, img.height
        if left < 0:
            x -= left
            w += left
            left = 0
        if top < 0:
            y -= top
            h += top
            top = 0
        if left + w > iw:
            w = iw - left
        if top + h > ih:
            h = ih - top
        if x < 0:
            left -= x
            w += x
            x = 0
        if y < 0:
            top -= y
            h += y
            y = 0
        if x + w > DWIDTH:
            w = DWIDTH - x
        if y + h > DHEIGHT:
            h = DHEIGHT - y
        if w <= 0 or h <= 0:
            return
        px = self.pixels
        for row in range(h):
            s_base = (top + row) * iw + left
            d_base = (y + row) * DWIDTH + x
            for col in range(w):
                val = src[s_base + col]
                if val is not None:
                    px[d_base + col] = val


class _HeadlessBackend:
    """Pure-Python fallback used only on non-Windows OS for automated testing."""

    def __init__(self) -> None:
        self.closed = False
        self._key_cv = threading.Condition()
        self.down_keys: set[int] = set()
        self.pressed_flips: set[int] = set()
        self.released_flips: set[int] = set()
        self.event_queue: collections.deque[KeyEvent] = collections.deque(maxlen=256)
        self.pixels = [0xFFFFFFFF] * (DWIDTH * DHEIGHT)
        self._zbuf = [0] * (DWIDTH * DHEIGHT)

    clear_zbuf = _Win32Backend.clear_zbuf
    tri_zbuf = _Win32Backend.tri_zbuf

    def pump(self) -> None:
        pass

    def present(self) -> None:
        pass

    def destroy(self) -> None:
        self.closed = True

    def clear(self, color: int) -> None:
        if color == C_NONE:
            return
        if color == C_INVERT:
            for i in range(len(self.pixels)):
                self.pixels[i] ^= 0x00FFFFFF
        else:
            v = _LUT_BGRA[color & 0xFFFF]
            self.pixels[:] = [v] * (DWIDTH * DHEIGHT)

    def rect(self, x1: int, y1: int, x2: int, y2: int, color: int) -> None:
        if color == C_NONE:
            return
        if x1 > x2:
            x1, x2 = x2, x1
        if y1 > y2:
            y1, y2 = y2, y1
        x1 = max(0, x1)
        y1 = max(0, y1)
        x2 = min(DWIDTH - 1, x2)
        y2 = min(DHEIGHT - 1, y2)
        if x1 > x2 or y1 > y2:
            return
        if color == C_INVERT:
            for y in range(y1, y2 + 1):
                b = y * DWIDTH
                for x in range(x1, x2 + 1):
                    self.pixels[b + x] ^= 0x00FFFFFF
        else:
            v = _LUT_BGRA[color & 0xFFFF]
            span = [v] * (x2 - x1 + 1)
            for y in range(y1, y2 + 1):
                b = y * DWIDTH
                self.pixels[b + x1 : b + x2 + 1] = span

    def pixel(self, x: int, y: int, color: int) -> None:
        if color != C_NONE and 0 <= x < DWIDTH and 0 <= y < DHEIGHT:
            idx = y * DWIDTH + x
            if color == C_INVERT:
                self.pixels[idx] ^= 0x00FFFFFF
            else:
                self.pixels[idx] = _LUT_BGRA[color & 0xFFFF]

    def getpixel(self, x: int, y: int) -> int:
        if 0 <= x < DWIDTH and 0 <= y < DHEIGHT:
            return _bgra_to_rgb565(self.pixels[y * DWIDTH + x])
        return 0

    def line(self, x1: int, y1: int, x2: int, y2: int, color: int) -> None:
        if color == C_NONE:
            return
        dx = abs(x2 - x1)
        sx = 1 if x1 < x2 else -1
        dy = -abs(y2 - y1)
        sy = 1 if y1 < y2 else -1
        err = dx + dy
        while True:
            self.pixel(x1, y1, color)
            if x1 == x2 and y1 == y2:
                break
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x1 += sx
            if e2 <= dx:
                err += dx
                y1 += sy

    def ellipse(self, x1: int, y1: int, x2: int, y2: int, fill: int, border: int) -> None:
        if fill != C_NONE:
            self.rect(x1, y1, x2, y2, fill)
        elif border != C_NONE:
            self.rect(x1, y1, x2, y2, border)

    def poly(self, vertices, fill: int, border: int) -> None:
        n = len(vertices) // 2
        if n < 2:
            return
        xs = [int(vertices[2 * i]) for i in range(n)]
        ys = [int(vertices[2 * i + 1]) for i in range(n)]
        if fill != C_NONE and n >= 3:
            y_min = max(0, min(ys))
            y_max = min(DHEIGHT - 1, max(ys))
            for y in range(y_min, y_max + 1):
                nodes = []
                j = n - 1
                for i in range(n):
                    yi, yj = ys[i], ys[j]
                    if (yi < y <= yj) or (yj < y <= yi):
                        nodes.append(int(xs[i] + (y - yi) * (xs[j] - xs[i]) / (yj - yi)))
                    j = i
                nodes.sort()
                for k in range(0, len(nodes) - 1, 2):
                    self.rect(nodes[k], y, nodes[k + 1], y, fill)
        if border != C_NONE:
            for i in range(n):
                j = (i + 1) % n
                self.line(xs[i], ys[i], xs[j], ys[j], border)

    def text_opt(
        self, x: int, y: int, fg: int, bg: int, halign: int, valign: int, s: str, size: int
    ) -> None:
        if bg >= 0 and s:
            txt = s[:size] if size >= 0 else s
            self.rect(x, y, x + len(txt) * 6, y + 10, bg)

    def subimage(self, x: int, y: int, img: image, left: int, top: int, w: int, h: int) -> None:
        for row in range(h):
            for col in range(w):
                sx, sy = left + col, top + row
                dx, dy = x + col, y + row
                if 0 <= sx < img.width and 0 <= sy < img.height and 0 <= dx < DWIDTH and 0 <= dy < DHEIGHT:
                    v = img._decoded[sy * img.width + sx]
                    if v is not None:
                        self.pixels[dy * DWIDTH + dx] = v


_backend: _Win32Backend | _HeadlessBackend | None = None


def _ensure() -> _Win32Backend | _HeadlessBackend:
    global _backend
    if _backend is None or _backend.closed:
        _backend = _Win32Backend(scale=2) if sys.platform == "win32" else _HeadlessBackend()
    return _backend


def _cleanup() -> None:
    global _backend
    if _backend is not None:
        _backend.destroy()
        _backend = None


atexit.register(_cleanup)


# ============================================================================
# 5. PUBLIC `gint` API FUNCTIONS
# ============================================================================
def dclear(color: int) -> None:
    _ensure().clear(int(color))


def dupdate() -> None:
    b = _ensure()
    if b.closed:
        raise SystemExit(0)
    b.present()


def drect(x1: int, y1: int, x2: int, y2: int, color: int) -> None:
    _ensure().rect(int(x1), int(y1), int(x2), int(y2), int(color))


def drect_border(
    x1: int, y1: int, x2: int, y2: int, fill_color: int, border_width: int, border_color: int
) -> None:
    b = _ensure()
    x1, y1, x2, y2 = int(x1), int(y1), int(x2), int(y2)
    bw = int(border_width)
    if x1 > x2:
        x1, x2 = x2, x1
    if y1 > y2:
        y1, y2 = y2, y1
    if fill_color != C_NONE:
        ix1, iy1, ix2, iy2 = x1 + bw, y1 + bw, x2 - bw, y2 - bw
        if ix1 <= ix2 and iy1 <= iy2:
            b.rect(ix1, iy1, ix2, iy2, int(fill_color))
    if bw > 0 and border_color != C_NONE:
        bc = int(border_color)
        b.rect(x1, y1, x2, min(y2, y1 + bw - 1), bc)
        b.rect(x1, max(y1, y2 - bw + 1), x2, y2, bc)
        if y1 + bw <= y2 - bw:
            b.rect(x1, y1 + bw, min(x2, x1 + bw - 1), y2 - bw, bc)
            b.rect(max(x1, x2 - bw + 1), y1 + bw, x2, y2 - bw, bc)


def dpixel(x: int, y: int, color: int) -> None:
    _ensure().pixel(int(x), int(y), int(color))


def dgetpixel(x: int, y: int) -> int:
    return _ensure().getpixel(int(x), int(y))


def dline(x1: int, y1: int, x2: int, y2: int, color: int) -> None:
    _ensure().line(int(x1), int(y1), int(x2), int(y2), int(color))


def dhline(y: int, color: int) -> None:
    _ensure().rect(0, int(y), DWIDTH - 1, int(y), int(color))


def dvline(x: int, color: int) -> None:
    _ensure().rect(int(x), 0, int(x), DHEIGHT - 1, int(color))


def dcircle(x: int, y: int, r: int, fill: int, border: int) -> None:
    x, y, r = int(x), int(y), int(r)
    _ensure().ellipse(x - r, y - r, x + r, y + r, int(fill), int(border))


def dellipse(x1: int, y1: int, x2: int, y2: int, fill: int, border: int) -> None:
    _ensure().ellipse(int(x1), int(y1), int(x2), int(y2), int(fill), int(border))


def dpoly(vertices, fill: int, border: int) -> None:
    _ensure().poly(vertices, int(fill), int(border))


def dclear_zbuf() -> None:
    _ensure().clear_zbuf()


def dtri_zbuf(
    x0: int, y0: int, z0: int,
    x1: int, y1: int, z1: int,
    x2: int, y2: int, z2: int,
    color: int,
    ymin: int = 0,
    ymax: int = DHEIGHT - 1,
) -> None:
    _ensure().tri_zbuf(
        int(x0), int(y0), int(z0),
        int(x1), int(y1), int(z1),
        int(x2), int(y2), int(z2),
        int(color), int(ymin), int(ymax),
    )


def dfont(new_font=None) -> None:
    _ensure()


def dtext(x: int, y: int, fg: int, text: str) -> None:
    _ensure().text_opt(int(x), int(y), int(fg), C_NONE, DTEXT_LEFT, DTEXT_TOP, str(text), -1)


def dtext_opt(
    x: int, y: int, fg: int, bg: int, halign: int, valign: int, text: str, size: int = -1
) -> None:
    _ensure().text_opt(
        int(x), int(y), int(fg), int(bg), int(halign), int(valign), str(text), int(size)
    )


def dimage(x: int, y: int, img: image) -> None:
    _ensure().subimage(int(x), int(y), img, 0, 0, img.width, img.height)


def dsubimage(
    x: int, y: int, img: image, left: int, top: int, width: int, height: int
) -> None:
    _ensure().subimage(int(x), int(y), img, int(left), int(top), int(width), int(height))


# Monochrome compatibility helpers
def dgray(value: int) -> bool:
    return True


def dgray_enabled() -> bool:
    return True


def dgray_setdelays(light: int, dark: int) -> None:
    pass


def dgray_getdelays() -> tuple[int, int]:
    return (1000, 1000)


# --- Thread-Safe Keyboard & Events ---
def clearevents() -> None:
    b = _ensure()
    with b._key_cv:
        b.event_queue.clear()
        b.pressed_flips.clear()
        b.released_flips.clear()


def cleareventflips() -> None:
    b = _ensure()
    with b._key_cv:
        b.pressed_flips.clear()
        b.released_flips.clear()


def pollevent() -> KeyEvent:
    b = _ensure()
    with b._key_cv:
        if b.event_queue:
            return b.event_queue.popleft()
        if b.closed:
            return KeyEvent(ticks_ms(), False, False, False, KEYEV_DOWN, KEY_EXIT, 0, 0)
    return KeyEvent(ticks_ms(), False, False, False, KEYEV_NONE, 0, 0, 0)


def keydown(key: int) -> bool:
    b = _ensure()
    with b._key_cv:
        if b.closed and key == KEY_EXIT:
            return True
        return int(key) in b.down_keys


def keydown_all(*keys: int) -> bool:
    b = _ensure()
    with b._key_cv:
        return all(int(k) in b.down_keys for k in keys)


def keydown_any(*keys: int) -> bool:
    b = _ensure()
    with b._key_cv:
        return any(int(k) in b.down_keys for k in keys)


def keypressed(key: int) -> bool:
    b = _ensure()
    with b._key_cv:
        return int(key) in b.pressed_flips


def keyreleased(key: int) -> bool:
    b = _ensure()
    with b._key_cv:
        return int(key) in b.released_flips


def getkey_opt(options: int = GETKEY_DEFAULT, timeout_ms: int | None = -1) -> KeyEvent:
    b = _ensure()
    b.present()
    has_timeout = (timeout_ms is not None) and (int(timeout_ms) >= 0)
    deadline = _stdlib_time.perf_counter() + (int(timeout_ms) * 0.001) if has_timeout else 0.0
    allow_rep = bool(options & (GETKEY_REP_ARROWS | GETKEY_REP_ALL))

    with b._key_cv:
        while True:
            if b.closed:
                raise SystemExit(0)
            while b.event_queue:
                ev = b.event_queue.popleft()
                if ev.type == KEYEV_UP:
                    continue
                if ev.key in (KEY_SHIFT, KEY_ALPHA) and (
                    options & (GETKEY_MOD_SHIFT | GETKEY_MOD_ALPHA)
                ):
                    continue
                if ev.type == KEYEV_HOLD and not allow_rep:
                    if ev.key not in (KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN):
                        continue
                sh = bool(ev.shift) if (options & GETKEY_MOD_SHIFT) else False
                al = bool(ev.alpha) if (options & GETKEY_MOD_ALPHA) else False
                return KeyEvent(ev.time, sh or al, sh, al, ev.type, ev.key, 0, 0)

            if has_timeout:
                rem = deadline - _stdlib_time.perf_counter()
                if rem <= 0:
                    return KeyEvent(ticks_ms(), False, False, False, KEYEV_NONE, 0, 0, 0)
                b._key_cv.wait(timeout=rem)
            else:
                b._key_cv.wait(timeout=0.05)


def getkey() -> KeyEvent:
    return getkey_opt(GETKEY_DEFAULT, -1)


def keycode_function(keycode: int) -> int:
    if KEY_F1 <= keycode <= KEY_F6:
        return (keycode - KEY_F1) + 1
    return -1


def keycode_digit(keycode: int) -> int:
    _map = {
        KEY_0: 0, KEY_1: 1, KEY_2: 2, KEY_3: 3, KEY_4: 4,
        KEY_5: 5, KEY_6: 6, KEY_7: 7, KEY_8: 8, KEY_9: 9,
    }
    return _map.get(int(keycode), -1)
