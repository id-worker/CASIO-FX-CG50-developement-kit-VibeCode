# obj_viewer.py -- 3D Wavefront .obj Viewer for PythonExtra (fx-CG50) & Windows IDLE (win_gint)
# Controls:
#   Arrows          : Look camera (Yaw Left/Right, Pitch Up/Down -- horizon stays level!)
#   8 / 5 / 4 / 6   : Move camera Forward / Back / Left / Right
#   1 / 0           : Move camera Up / Down (world vertical)
#   SHIFT (hold)    : 3x faster camera movement ([FST])
#   ALPHA (hold/tap): 5x slower precision movement ([SLW]; rotation stays normal; Ctrl in IDLE)
#   F5 / F6         : Roll camera around optical center (CCW / CW)
#   + / -           : Image size (zoom in / out)
#   F1              : Toggle Solid CULL (Half-Lambert gray shading) <-> Wireframe
#   F2              : Flip normals (invert face culling & lighting)
#   F3              : Reset camera to frame object
#   F4              : Open another .obj file
#   EXIT            : Back to file prompt / Quit

import gint
import math
import sys
import time
import gc
from array import array

W = gint.DWIDTH     # 396 on PythonExtra / win_gint
H = gint.DHEIGHT    # 224 on PythonExtra / win_gint
VIEW_TOP = 16
VIEW_BOT = H - 15
CX = W // 2
CY = (VIEW_TOP + VIEW_BOT) // 2
MAX_PITCH = 1.553343  # ~89 deg

_IS_PC = (getattr(sys, "platform", "") == "win32") or (
    getattr(getattr(sys, "implementation", None), "name", "") == "cpython"
)

# 32-level neutral RGB565 grayscale palette (0 = black .. 31 = white)
GRAY = [gint.C_RGB(i, i, i) for i in range(32)]
COL_BG = GRAY[29]
COL_HDR = gint.C_BLACK
COL_WHITE = gint.C_WHITE
COL_WIRE = GRAY[3]
COL_ACCENT = gint.C_RGB(4, 12, 26)

# Adaptive resident vs streaming limits (PC keeps huge meshes resident in RAM)
MAX_RAM_VERTS = 120000 if _IS_PC else 4200
MAX_RAM_TRIS = 200000 if _IS_PC else 6500
MAX_QUANT_VERTS = 22000
WIN_VERTS = 10000
STREAM_BATCH = 480
READ_CHUNK = 4096


def _now_ms():
    if hasattr(time, "ticks_ms"):
        return time.ticks_ms()
    return int(time.monotonic() * 1000)


def _diff_ms(t1, t0):
    if hasattr(time, "ticks_diff"):
        return time.ticks_diff(t1, t0)
    return t1 - t0


# ---------------------------------------------------------------------------
# Graphical filename prompt
# ---------------------------------------------------------------------------
_LETTER_MAP = {
    gint.KEY_XOT: "a", gint.KEY_LOG: "b", gint.KEY_LN: "c",
    gint.KEY_SIN: "d", gint.KEY_COS: "e", gint.KEY_TAN: "f",
    gint.KEY_FRAC: "g", gint.KEY_FD: "h", gint.KEY_LEFTP: "i",
    gint.KEY_RIGHTP: "j", gint.KEY_COMMA: "k", gint.KEY_ARROW: "l",
    gint.KEY_7: "m", gint.KEY_8: "n", gint.KEY_9: "o",
    gint.KEY_4: "p", gint.KEY_5: "q", gint.KEY_6: "r",
    gint.KEY_MUL: "s", gint.KEY_DIV: "t",
    gint.KEY_1: "u", gint.KEY_2: "v", gint.KEY_3: "w",
    gint.KEY_ADD: "x", gint.KEY_SUB: "y",
    gint.KEY_0: "z",
}

_DIGIT_MAP = {
    gint.KEY_0: "0", gint.KEY_1: "1", gint.KEY_2: "2",
    gint.KEY_3: "3", gint.KEY_4: "4", gint.KEY_5: "5",
    gint.KEY_6: "6", gint.KEY_7: "7", gint.KEY_8: "8",
    gint.KEY_9: "9", gint.KEY_DOT: ".", gint.KEY_SUB: "-",
    gint.KEY_NEG: "_", gint.KEY_DIV: "/",
}


def ask_obj_filename(err_msg=""):
    gint.clearevents()
    buf = ""
    mode = 0  # 0 = abc, 1 = ABC, 2 = 123
    mode_names = ("abc", "ABC", "123")

    while True:
        gint.dclear(COL_BG)
        gint.drect(0, 0, W - 1, 17, COL_HDR)
        gint.dtext(6, 3, COL_WHITE, "3D .OBJ Viewer v2.1 (gint)")
        gint.drect(W - 42, 2, W - 5, 15, COL_ACCENT)
        gint.dtext(W - 36, 3, COL_WHITE, mode_names[mode])

        gint.dtext(18, 36, GRAY[4], "Enter .obj file name (without .obj):")
        gint.drect_border(18, 56, W - 19, 82, COL_WHITE, 1, COL_HDR)

        shown = buf[-34:] if len(buf) > 34 else buf
        gint.dtext(24, 63, COL_HDR, shown + "_")
        gint.dtext(24, 88, GRAY[12], "Target: /" + (buf if buf else "<name>") + ".obj")

        if err_msg:
            gint.dtext(18, 112, gint.C_RED, err_msg[:46])

        gint.dtext(18, 140, GRAY[9], "EXE: Open   DEL: Erase   F3: abc/ABC/123")
        gint.dtext(18, 156, GRAY[9], "F4: '_'     F5: '/'      EXIT/F6: Quit")

        labels = ("OPEN", "", "a/A/1", "_", "/", "QUIT")
        gint.drect(0, H - 18, W - 1, H - 1, COL_WHITE)
        gint.dhline(H - 19, GRAY[18])
        for i in range(6):
            if labels[i]:
                x1 = i * 64 + 6
                x2 = (i + 1) * 64 + 1
                gint.drect_border(x1, H - 16, x2, H - 2,
                                  COL_HDR if i == 0 else COL_WHITE, 1, COL_HDR)
                gint.dtext_opt((x1 + x2) // 2, H - 9,
                               COL_WHITE if i == 0 else COL_HDR, gint.C_NONE,
                               gint.DTEXT_CENTER, gint.DTEXT_MIDDLE, labels[i], -1)
        gint.dupdate()

        ev = gint.getkey_opt(gint.GETKEY_MOD_SHIFT | gint.GETKEY_MOD_ALPHA, -1)
        k = ev.key

        if k == gint.KEY_EXIT or k == gint.KEY_F6:
            return None
        if k == gint.KEY_EXE or k == gint.KEY_F1:
            name = buf.strip()
            if name:
                return name
            err_msg = "Please type a file name first"
            continue
        if k == gint.KEY_DEL:
            buf = buf[:-1]
            continue
        if k == gint.KEY_ALPHA or k == gint.KEY_F3:
            mode = (mode + 1) % 3
            continue
        if k == gint.KEY_F4 or k == gint.KEY_NEG:
            buf += "_"
            continue
        if k == gint.KEY_F5:
            buf += "/"
            continue

        ch = ""
        if ev.shift and k in _DIGIT_MAP:
            ch = _DIGIT_MAP[k]
        elif mode == 2:
            ch = _DIGIT_MAP.get(k, "")
        else:
            ch = _LETTER_MAP.get(k, "")
            if ch and (mode == 1 or ev.shift):
                ch = ch.upper()
            elif not ch and k in _DIGIT_MAP:
                ch = _DIGIT_MAP[k]
        if ch and len(buf) < 60:
            buf += ch
            err_msg = ""


def resolve_obj_path(raw_name):
    s = raw_name.strip().replace("\\", "/")
    while s.startswith("/"):
        s = s[1:]
    base = s[:-4] if s.lower().endswith(".obj") else s
    candidates = (
        base + ".obj",
        base + ".OBJ",
        "/" + base + ".obj",
        "/" + base + ".OBJ",
        s,
        "/" + s,
    )
    for p in candidates:
        try:
            f = open(p, "r")
            f.close()
            return p
        except OSError:
            pass
    return None


def iter_obj_lines(path):
    f = open(path, "r")
    try:
        tail = ""
        while True:
            chunk = f.read(READ_CHUNK)
            if not chunk:
                if tail:
                    yield tail
                break
            data = tail + chunk
            parts = data.split("\n")
            tail = parts.pop()
            i = 0
            n = len(parts)
            while i < n:
                ln = parts[i].rstrip("\r")
                while ln.endswith("\\") and i + 1 < n:
                    i += 1
                    ln = ln[:-1] + " " + parts[i].rstrip("\r")
                yield ln
                i += 1
    finally:
        f.close()


def _parse_face_v(tok, v_seen):
    slash = tok.find("/")
    if slash >= 0:
        tok = tok[:slash]
    idx = int(tok)
    return (v_seen + idx) if idx < 0 else (idx - 1)


def draw_status_banner(title, sub):
    gint.dclear(COL_BG)
    gint.drect(0, 0, W - 1, 17, COL_HDR)
    gint.dtext(6, 3, COL_WHITE, title[:44])
    gint.drect_border(36, 84, W - 37, 134, COL_WHITE, 1, COL_HDR)
    gint.dtext(48, 95, COL_HDR, sub[:40])
    gint.dupdate()


def _alloc_arr(tc, count, item_bytes):
    if count <= 0:
        return array(tc)
    b = bytearray(count * item_bytes)
    a = array(tc, b)
    del b
    return a


class ObjModel:
    def __init__(self, path):
        self.path = path
        self.mode = "ram"
        self.n_verts = 0
        self.n_tris = 0
        self.cx = 0.0
        self.cy = 0.0
        self.cz = 0.0
        self.radius = 1.0
        self.q_scale = 1.0

        # Resident arrays ("ram") + precomputed unit world normals
        self.vx = None
        self.vy = None
        self.vz = None
        self.fa = None
        self.fb = None
        self.fc = None
        self.fnx = None
        self.fny = None
        self.fnz = None

        # Stream fallback arrays
        self.qx = None
        self.qy = None
        self.qz = None
        self.pvx = None
        self.pvy = None
        self.pvz = None
        self.p_tris = 0


def load_obj_model(path):
    gc.collect()
    m = ObjModel(path)
    draw_status_banner("Scanning " + path, "Pass 1: bounds & counts...")

    min_x = min_y = min_z = 1e30
    max_x = max_y = max_z = -1e30
    nv = 0
    nt = 0
    step_tick = 0

    for raw in iter_obj_lines(path):
        s = raw.lstrip()
        if not s or s[0] == "#":
            continue
        c0 = s[0]
        if c0 == "v" and len(s) > 1 and (s[1] == " " or s[1] == "\t"):
            parts = s.split()
            if len(parts) >= 4:
                x = float(parts[1])
                y = float(parts[2])
                z = float(parts[3])
                if x < min_x: min_x = x
                if x > max_x: max_x = x
                if y < min_y: min_y = y
                if y > max_y: max_y = y
                if z < min_z: min_z = z
                if z > max_z: max_z = z
                nv += 1
        elif c0 == "f" and len(s) > 1 and (s[1] == " " or s[1] == "\t"):
            parts = s.split()
            k = len(parts) - 1
            if k >= 3:
                nt += k - 2

        step_tick += 1
        if (step_tick & 1023) == 0:
            draw_status_banner("Scanning " + path,
                               "V: " + str(nv) + "   Tris: " + str(nt))

    if nv == 0 or nt == 0:
        return None

    m.n_verts = nv
    m.n_tris = nt
    m.cx = 0.5 * (min_x + max_x)
    m.cy = 0.5 * (min_y + max_y)
    m.cz = 0.5 * (min_z + max_z)
    dx = max_x - min_x
    dy = max_y - min_y
    dz = max_z - min_z
    span = max(dx, dy, dz, 1e-6)
    m.radius = max(0.5 * math.sqrt(dx * dx + dy * dy + dz * dz), 1e-6)
    m.q_scale = max((0.5 * span) / 32000.0, 1e-9)

    gc.collect()
    if nv <= MAX_RAM_VERTS and nt <= MAX_RAM_TRIS:
        try:
            _load_ram_mode(m)
            return m
        except MemoryError:
            m.vx = m.vy = m.vz = m.fa = m.fb = m.fc = None
            m.fnx = m.fny = m.fnz = None
            gc.collect()

    _load_stream_quant_mode(m)
    return m


def _load_ram_mode(m):
    m.mode = "ram"
    nv = m.n_verts
    nt = m.n_tris
    draw_status_banner("Loading " + m.path,
                       "RAM mode: V=" + str(nv) + " F=" + str(nt))
    vx = _alloc_arr("f", nv, 4)
    vy = _alloc_arr("f", nv, 4)
    vz = _alloc_arr("f", nv, 4)
    idx_tc = "I" if nv > 65535 else "H"
    idx_sz = 4 if nv > 65535 else 2
    fa = _alloc_arr(idx_tc, nt, idx_sz)
    fb = _alloc_arr(idx_tc, nt, idx_sz)
    fc = _alloc_arr(idx_tc, nt, idx_sz)
    fnx = _alloc_arr("b", nt, 1)
    fny = _alloc_arr("b", nt, 1)
    fnz = _alloc_arr("b", nt, 1)
    gc.collect()

    cx, cy, cz = m.cx, m.cy, m.cz
    vi = 0
    ti = 0
    sqrt = math.sqrt
    for raw in iter_obj_lines(m.path):
        s = raw.lstrip()
        if not s or s[0] == "#":
            continue
        c0 = s[0]
        if c0 == "v" and len(s) > 1 and (s[1] == " " or s[1] == "\t"):
            p = s.split()
            if len(p) >= 4 and vi < nv:
                vx[vi] = float(p[1]) - cx
                vy[vi] = float(p[2]) - cy
                vz[vi] = float(p[3]) - cz
                vi += 1
        elif c0 == "f" and len(s) > 1 and (s[1] == " " or s[1] == "\t"):
            p = s.split()
            n = len(p) - 1
            if n >= 3:
                i0 = _parse_face_v(p[1], vi)
                prev = _parse_face_v(p[2], vi)
                for j in range(3, n + 1):
                    cur = _parse_face_v(p[j], vi)
                    if 0 <= i0 < vi and 0 <= prev < vi and 0 <= cur < vi and ti < nt:
                        e1x = vx[prev] - vx[i0]
                        e1y = vy[prev] - vy[i0]
                        e1z = vz[prev] - vz[i0]
                        e2x = vx[cur] - vx[i0]
                        e2y = vy[cur] - vy[i0]
                        e2z = vz[cur] - vz[i0]
                        nx = e1y * e2z - e1z * e2y
                        ny = e1z * e2x - e1x * e2z
                        nz = e1x * e2y - e1y * e2x
                        nl = sqrt(nx * nx + ny * ny + nz * nz)
                        if nl > 1e-11:
                            inv = 127.0 / nl
                            fa[ti] = i0
                            fb[ti] = prev
                            fc[ti] = cur
                            fnx[ti] = int(nx * inv)
                            fny[ti] = int(ny * inv)
                            fnz[ti] = int(nz * inv)
                            ti += 1
                    prev = cur
    m.n_tris = ti
    m.vx, m.vy, m.vz = vx, vy, vz
    m.fa, m.fb, m.fc = fa, fb, fc
    m.fnx, m.fny, m.fnz = fnx, fny, fnz
    gc.collect()


def _load_stream_quant_mode(m):
    m.mode = "stream"
    nv = m.n_verts
    draw_status_banner("Indexing " + m.path,
                       "Stream: V=" + str(nv) + " F=" + str(m.n_tris))
    qx = _alloc_arr("h", nv, 2)
    qy = _alloc_arr("h", nv, 2)
    qz = _alloc_arr("h", nv, 2)
    gc.collect()

    cx, cy, cz = m.cx, m.cy, m.cz
    inv_q = 1.0 / m.q_scale
    qs = m.q_scale
    vi = 0
    max_pv = min(300, m.n_tris)
    stride = max(1, m.n_tris // max_pv)
    pvx = array("f")
    pvy = array("f")
    pvz = array("f")
    seen_t = 0
    pv_cnt = 0

    for raw in iter_obj_lines(m.path):
        s = raw.lstrip()
        if not s or s[0] == "#":
            continue
        c0 = s[0]
        if c0 == "v" and len(s) > 1 and (s[1] == " " or s[1] == "\t"):
            p = s.split()
            if len(p) >= 4 and vi < nv:
                qx[vi] = max(-32767, min(32767, int((float(p[1]) - cx) * inv_q)))
                qy[vi] = max(-32767, min(32767, int((float(p[2]) - cy) * inv_q)))
                qz[vi] = max(-32767, min(32767, int((float(p[3]) - cz) * inv_q)))
                vi += 1
        elif c0 == "f" and len(s) > 1 and (s[1] == " " or s[1] == "\t"):
            p = s.split()
            n = len(p) - 1
            if n >= 3:
                i0 = _parse_face_v(p[1], vi)
                prev = _parse_face_v(p[2], vi)
                for j in range(3, n + 1):
                    cur = _parse_face_v(p[j], vi)
                    if seen_t % stride == 0 and pv_cnt < max_pv and 0 <= i0 < vi and 0 <= prev < vi and 0 <= cur < vi:
                        for idx in (i0, prev, cur):
                            pvx.append(qx[idx] * qs)
                            pvy.append(qy[idx] * qs)
                            pvz.append(qz[idx] * qs)
                        pv_cnt += 1
                    seen_t += 1
                    prev = cur

    m.qx, m.qy, m.qz = qx, qy, qz
    m.pvx, m.pvy, m.pvz = pvx, pvy, pvz
    m.p_tris = pv_cnt
    gc.collect()


# ---------------------------------------------------------------------------
# Artifact-Free 3D Near-Plane + 2D Sutherland-Hodgman Polygon Clipper
# (Eliminates broken/twisted triangles caused by raw coordinate clamping!)
# ---------------------------------------------------------------------------
def _clip_poly_2d(pts):
    # Clip flat [x0, y0, x1, y1, ...] polygon against [0..W-1] x [VIEW_TOP..VIEW_BOT]
    # 1. Left: x >= 0
    out = []
    n = len(pts)
    if n < 6:
        return None
    px, py = pts[n - 2], pts[n - 1]
    pin = px >= 0.0
    for i in range(0, n, 2):
        cx, cy = pts[i], pts[i + 1]
        cin = cx >= 0.0
        if cin != pin:
            out.append(0.0)
            out.append(py + (0.0 - px) * (cy - py) / (cx - px))
        if cin:
            out.append(cx)
            out.append(cy)
        px, py, pin = cx, cy, cin
    pts = out

    # 2. Right: x <= W - 1
    xmax = float(W - 1)
    out = []
    n = len(pts)
    if n < 6:
        return None
    px, py = pts[n - 2], pts[n - 1]
    pin = px <= xmax
    for i in range(0, n, 2):
        cx, cy = pts[i], pts[i + 1]
        cin = cx <= xmax
        if cin != pin:
            out.append(xmax)
            out.append(py + (xmax - px) * (cy - py) / (cx - px))
        if cin:
            out.append(cx)
            out.append(cy)
        px, py, pin = cx, cy, cin
    pts = out

    # 3. Top: y >= VIEW_TOP
    ymin = float(VIEW_TOP)
    out = []
    n = len(pts)
    if n < 6:
        return None
    px, py = pts[n - 2], pts[n - 1]
    pin = py >= ymin
    for i in range(0, n, 2):
        cx, cy = pts[i], pts[i + 1]
        cin = cy >= ymin
        if cin != pin:
            out.append(px + (ymin - py) * (cx - px) / (cy - py))
            out.append(ymin)
        if cin:
            out.append(cx)
            out.append(cy)
        px, py, pin = cx, cy, cin
    pts = out

    # 4. Bottom: y <= VIEW_BOT
    ymax = float(VIEW_BOT)
    out = []
    n = len(pts)
    if n < 6:
        return None
    px, py = pts[n - 2], pts[n - 1]
    pin = py <= ymax
    for i in range(0, n, 2):
        cx, cy = pts[i], pts[i + 1]
        cin = cy <= ymax
        if cin != pin:
            out.append(px + (ymax - py) * (cx - px) / (cy - py))
            out.append(ymax)
        if cin:
            out.append(cx)
            out.append(cy)
        px, py, pin = cx, cy, cin

    if len(out) < 6:
        return None
    return [int(v + 0.5) for v in out]


def clip_and_project_tri(x0, y0, z0, x1, y1, z1, x2, y2, z2, near_z, focal):
    in0 = z0 >= near_z
    in1 = z1 >= near_z
    in2 = z2 >= near_z
    cnt = (1 if in0 else 0) + (1 if in1 else 0) + (1 if in2 else 0)
    if cnt == 0:
        return None

    if cnt == 3:
        sx0 = CX + x0 * (focal / z0)
        sy0 = CY - y0 * (focal / z0)
        sx1 = CX + x1 * (focal / z1)
        sy1 = CY - y1 * (focal / z1)
        sx2 = CX + x2 * (focal / z2)
        sy2 = CY - y2 * (focal / z2)
        if (sx0 < 0 and sx1 < 0 and sx2 < 0) or (sx0 >= W and sx1 >= W and sx2 >= W):
            return None
        if (sy0 < VIEW_TOP and sy1 < VIEW_TOP and sy2 < VIEW_TOP) or (
            sy0 > VIEW_BOT and sy1 > VIEW_BOT and sy2 > VIEW_BOT
        ):
            return None
        # Fast path when completely inside viewport
        if (0.0 <= sx0 < W and 0.0 <= sx1 < W and 0.0 <= sx2 < W and
                VIEW_TOP <= sy0 <= VIEW_BOT and VIEW_TOP <= sy1 <= VIEW_BOT and VIEW_TOP <= sy2 <= VIEW_BOT):
            return [int(sx0 + 0.5), int(sy0 + 0.5),
                    int(sx1 + 0.5), int(sy1 + 0.5),
                    int(sx2 + 0.5), int(sy2 + 0.5)]
        return _clip_poly_2d([sx0, sy0, sx1, sy1, sx2, sy2])

    verts = ((x0, y0, z0, in0), (x1, y1, z1, in1), (x2, y2, z2, in2))
    proj = []
    px, py, pz, pin = verts[2]
    for cx, cy, cz, cin in verts:
        if cin != pin:
            t = (near_z - pz) / (cz - pz)
            ix = px + t * (cx - px)
            iy = py + t * (cy - py)
            inv = focal / near_z
            proj.append(CX + ix * inv)
            proj.append(CY - iy * inv)
        if cin:
            inv = focal / cz
            proj.append(CX + cx * inv)
            proj.append(CY - cy * inv)
        px, py, pz, pin = cx, cy, cz, cin

    return _clip_poly_2d(proj)


def shade_and_cull_cam(x0, y0, z0, x1, y1, z1, x2, y2, z2, flip_n, wire):
    if wire:
        return COL_WIRE
    e1x = x1 - x0; e1y = y1 - y0; e1z = z1 - z0
    e2x = x2 - x0; e2y = y2 - y0; e2z = z2 - z0
    nx = e1y * e2z - e1z * e2y
    ny = e1z * e2x - e1x * e2z
    nz = e1x * e2y - e1y * e2x
    if (nx * x0 + ny * y0 + nz * z0) * flip_n <= 0.0:
        return -1
    nlen = math.sqrt(nx * nx + ny * ny + nz * nz)
    if nlen < 1e-12:
        return -1
    s = -flip_n / nlen
    dot_l = (nx * s) * 0.35 + (ny * s) * 0.50 - (nz * s) * 0.79
    h = 0.5 * (dot_l + 1.0)
    if h < 0.0: h = 0.0
    elif h > 1.0: h = 1.0
    lvl = 6 + int(h * 21.0)
    return GRAY[27 if lvl > 27 else lvl]


def compute_cam_basis(yaw, pitch, roll):
    cy = math.cos(yaw)
    sy = math.sin(yaw)
    cp = math.cos(pitch)
    sp = math.sin(pitch)
    cr = math.cos(roll)
    sr = math.sin(roll)

    fx = sy * cp
    fy = sp
    fz = -cy * cp

    r0x, r0y, r0z = cy, 0.0, sy
    u0x, u0y, u0z = -sy * sp, cp, cy * sp

    rx = r0x * cr + u0x * sr
    ry = r0y * cr + u0y * sr
    rz = r0z * cr + u0z * sr

    ux = -r0x * sr + u0x * cr
    uy = -r0y * sr + u0y * cr
    uz = -r0z * sr + u0z * cr
    return rx, ry, rz, ux, uy, uz, fx, fy, fz


def draw_hud(model, wire, flip_n, speed_tag="", extra=""):
    gint.drect(0, 0, W - 1, VIEW_TOP - 1, COL_HDR)
    mode_tag = "WIRE" if wire else "CULL"
    n_tag = "N-" if flip_n < 0 else "N+"
    sp = (" " + speed_tag) if speed_tag else ""
    left = model.path.split("/")[-1][:13] + " [" + mode_tag + " " + n_tag + sp + "]"
    gint.dtext(4, 2, COL_WHITE, left)
    right = extra if extra else ("V:" + str(model.n_verts) + " F:" + str(model.n_tris))
    gint.dtext_opt(W - 4, 2, GRAY[24], gint.C_NONE,
                   gint.DTEXT_RIGHT, gint.DTEXT_TOP, right[:24], -1)

    gint.drect(0, VIEW_BOT + 1, W - 1, H - 1, GRAY[26])
    gint.dhline(VIEW_BOT + 1, GRAY[18])
    gint.dtext(4, H - 12, GRAY[4],
               "F1:Wire F2:Flip F3:Rst F4:Open ALPHA:Slow F5/6:Roll")


def render_ram_frame(m, cx, cy, cz, basis, wire, flip_n, focal, speed_tag=""):
    t0 = _now_ms()
    rx, ry, rz, ux, uy, uz, fx, fy, fz = basis
    vx, vy, vz = m.vx, m.vy, m.vz
    nv = m.n_verts
    near_z = max(0.025 * m.radius, 1e-4)

    tx = _alloc_arr("f", nv, 4)
    ty = _alloc_arr("f", nv, 4)
    tz = _alloc_arr("f", nv, 4)
    for i in range(nv):
        dx = vx[i] - cx
        dy = vy[i] - cy
        dz = vz[i] - cz
        tx[i] = dx * rx + dy * ry + dz * rz
        ty[i] = dx * ux + dy * uy + dz * uz
        tz[i] = dx * fx + dy * fy + dz * fz

    fa, fb, fc = m.fa, m.fb, m.fc
    fnx, fny, fnz = m.fnx, m.fny, m.fnz
    nt = m.n_tris

    # Right-handed world headlight (+flip_n!)
    ls = flip_n / 127.0
    lx_w = (0.35 * rx + 0.50 * ux - 0.79 * fx) * ls
    ly_w = (0.35 * ry + 0.50 * uy - 0.79 * fy) * ls
    lz_w = (0.35 * rz + 0.50 * uz - 0.79 * fz) * ls

    gint.dclear(COL_BG)

    if wire:
        for t in range(nt):
            a = fa[t]; b = fb[t]; c = fc[t]
            pts = clip_and_project_tri(
                tx[a], ty[a], tz[a],
                tx[b], ty[b], tz[b],
                tx[c], ty[c], tz[c], near_z, focal)
            if pts is not None:
                gint.dpoly(pts, gint.C_NONE, COL_WIRE)
    elif hasattr(gint, "dtri_zbuf"):
        # True per-pixel 16-bit Z-buffer on win_gint (zero broken/intersecting tris!)
        gint.dclear_zbuf()
        tri_zbuf = gint.dtri_zbuf
        z_scale = near_z * 65534.0
        for t in range(nt):
            a = fa[t]; b = fb[t]; c = fc[t]
            z0 = tz[a]; z1 = tz[b]; z2 = tz[c]
            if z0 < near_z and z1 < near_z and z2 < near_z:
                continue
            x0 = tx[a]; y0 = ty[a]
            x1 = tx[b]; y1 = ty[b]
            x2 = tx[c]; y2 = ty[c]

            e1x = x1 - x0; e1y = y1 - y0; e1z = z1 - z0
            e2x = x2 - x0; e2y = y2 - y0; e2z = z2 - z0
            nx = e1y * e2z - e1z * e2y
            ny = e1z * e2x - e1x * e2z
            nz = e1x * e2y - e1y * e2x
            if (nx * x0 + ny * y0 + nz * z0) * flip_n <= 0.0:
                continue

            dot_l = fnx[t] * lx_w + fny[t] * ly_w + fnz[t] * lz_w
            h = 0.5 * (dot_l + 1.0)
            if h < 0.0: h = 0.0
            elif h > 1.0: h = 1.0
            lvl = 6 + int(h * 21.0)
            col = GRAY[27 if lvl > 27 else lvl]

            # 3D near-plane clip -> 1 or 2 Z-buffered triangles
            in0 = z0 >= near_z; in1 = z1 >= near_z; in2 = z2 >= near_z
            if in0 and in1 and in2:
                sx0 = int(CX + x0 * (focal / z0) + 0.5)
                sy0 = int(CY - y0 * (focal / z0) + 0.5)
                sx1 = int(CX + x1 * (focal / z1) + 0.5)
                sy1 = int(CY - y1 * (focal / z1) + 0.5)
                sx2 = int(CX + x2 * (focal / z2) + 0.5)
                sy2 = int(CY - y2 * (focal / z2) + 0.5)
                if (sx0 < 0 and sx1 < 0 and sx2 < 0) or (sx0 >= W and sx1 >= W and sx2 >= W):
                    continue
                if (sy0 < VIEW_TOP and sy1 < VIEW_TOP and sy2 < VIEW_TOP) or (
                    sy0 > VIEW_BOT and sy1 > VIEW_BOT and sy2 > VIEW_BOT
                ):
                    continue
                iz0 = max(1, min(65535, int(z_scale / z0) + 1))
                iz1 = max(1, min(65535, int(z_scale / z1) + 1))
                iz2 = max(1, min(65535, int(z_scale / z2) + 1))
                tri_zbuf(sx0, sy0, iz0, sx1, sy1, iz1, sx2, sy2, iz2, col, VIEW_TOP, VIEW_BOT)
            else:
                verts3 = ((x0, y0, z0, in0), (x1, y1, z1, in1), (x2, y2, z2, in2))
                cpts = []
                px, py, pz, pin = verts3[2]
                for vx_c, vy_c, vz_c, cin in verts3:
                    if cin != pin:
                        tt = (near_z - pz) / (vz_c - pz)
                        ix = px + tt * (vx_c - px)
                        iy = py + tt * (vy_c - py)
                        inv = focal / near_z
                        cpts.append((int(CX + ix * inv + 0.5), int(CY - iy * inv + 0.5), 65535))
                    if cin:
                        inv = focal / vz_c
                        iz = max(1, min(65535, int(z_scale / vz_c) + 1))
                        cpts.append((int(CX + vx_c * inv + 0.5), int(CY - vy_c * inv + 0.5), iz))
                    px, py, pz, pin = vx_c, vy_c, vz_c, cin
                if len(cpts) >= 3:
                    p0, p1, p2 = cpts[0], cpts[1], cpts[2]
                    tri_zbuf(p0[0], p0[1], p0[2], p1[0], p1[1], p1[2], p2[0], p2[1], p2[2], col, VIEW_TOP, VIEW_BOT)
                    if len(cpts) == 4:
                        p3 = cpts[3]
                        tri_zbuf(p0[0], p0[1], p0[2], p2[0], p2[1], p2[2], p3[0], p3[1], p3[2], col, VIEW_TOP, VIEW_BOT)
    else:
        draw_list = []
        append = draw_list.append
        for t in range(nt):
            a = fa[t]; b = fb[t]; c = fc[t]
            z0 = tz[a]; z1 = tz[b]; z2 = tz[c]
            if z0 < near_z and z1 < near_z and z2 < near_z:
                continue
            x0 = tx[a]; y0 = ty[a]
            x1 = tx[b]; y1 = ty[b]
            x2 = tx[c]; y2 = ty[c]

            # Exact camera-space backface cull
            e1x = x1 - x0; e1y = y1 - y0; e1z = z1 - z0
            e2x = x2 - x0; e2y = y2 - y0; e2z = z2 - z0
            nx = e1y * e2z - e1z * e2y
            ny = e1z * e2x - e1x * e2z
            nz = e1x * e2y - e1y * e2x
            if (nx * x0 + ny * y0 + nz * z0) * flip_n <= 0.0:
                continue

            pts = clip_and_project_tri(x0, y0, z0, x1, y1, z1, x2, y2, z2, near_z, focal)
            if pts is None:
                continue

            # Bright Valve Half-Lambert shading from precomputed world normal
            dot_l = fnx[t] * lx_w + fny[t] * ly_w + fnz[t] * lz_w
            h = 0.5 * (dot_l + 1.0)
            if h < 0.0: h = 0.0
            elif h > 1.0: h = 1.0
            lvl = 6 + int(h * 21.0)
            col = GRAY[27 if lvl > 27 else lvl]

            # Weighted farthest- + average-depth key reduces large-wall overlap errors
            zmax = z0 if z0 > z1 else z1
            if z2 > zmax:
                zmax = z2
            depth_key = (z0 + z1 + z2) + 3.0 * zmax
            append((depth_key, col, pts))

        draw_list.sort(key=lambda it: -it[0])
        for _, col, pts in draw_list:
            gint.dpoly(pts, col, gint.C_NONE)
        del draw_list

    del tx, ty, tz
    ms = _diff_ms(_now_ms(), t0)
    draw_hud(m, wire, flip_n, speed_tag, str(ms) + "ms V:" + str(m.n_verts) + " F:" + str(m.n_tris))
    gint.dupdate()


_MOVE_KEYS = (
    gint.KEY_UP, gint.KEY_DOWN, gint.KEY_LEFT, gint.KEY_RIGHT,
    gint.KEY_8, gint.KEY_5, gint.KEY_4, gint.KEY_6,
    gint.KEY_1, gint.KEY_0,
    gint.KEY_F5, gint.KEY_F6,
    gint.KEY_ADD, gint.KEY_SUB,
)


def any_motion_key():
    for k in _MOVE_KEYS:
        if gint.keydown(k):
            return True
    return False


def render_preview_proxy(m, cx, cy, cz, basis, wire, flip_n, focal, speed_tag=""):
    rx, ry, rz, ux, uy, uz, fx, fy, fz = basis
    pvx, pvy, pvz = m.pvx, m.pvy, m.pvz
    near_z = max(0.025 * m.radius, 1e-4)
    gint.dclear(COL_BG)

    for t in range(m.p_tris):
        k = t * 3
        dx0 = pvx[k] - cx; dy0 = pvy[k] - cy; dz0 = pvz[k] - cz
        dx1 = pvx[k + 1] - cx; dy1 = pvy[k + 1] - cy; dz1 = pvz[k + 1] - cz
        dx2 = pvx[k + 2] - cx; dy2 = pvy[k + 2] - cy; dz2 = pvz[k + 2] - cz
        pts = clip_and_project_tri(
            dx0 * rx + dy0 * ry + dz0 * rz, dx0 * ux + dy0 * uy + dz0 * uz, dx0 * fx + dy0 * fy + dz0 * fz,
            dx1 * rx + dy1 * ry + dz1 * rz, dx1 * ux + dy1 * uy + dz1 * uz, dx1 * fx + dy1 * fy + dz1 * fz,
            dx2 * rx + dy2 * ry + dz2 * rz, dx2 * ux + dy2 * uy + dz2 * uz, dx2 * fx + dy2 * fy + dz2 * fz,
            near_z, focal)
        if pts is not None:
            gint.dpoly(pts, gint.C_NONE, COL_WIRE)

    draw_hud(m, wire, flip_n, speed_tag, "MOVING...")
    gint.dupdate()


def render_stream_full(m, cx, cy, cz, basis, wire, flip_n, focal, speed_tag=""):
    rx, ry, rz, ux, uy, uz, fx, fy, fz = basis
    near_z = max(0.025 * m.radius, 1e-4)
    gint.dclear(COL_BG)
    qx, qy, qz = m.qx, m.qy, m.qz
    qs = m.q_scale
    nv = m.n_verts
    vi = 0
    batch = []

    for raw in iter_obj_lines(m.path):
        s = raw.lstrip()
        if not s or s[0] == "#":
            continue
        c0 = s[0]
        if c0 == "v" and len(s) > 1 and (s[1] == " " or s[1] == "\t"):
            vi += 1
        elif c0 == "f" and len(s) > 1 and (s[1] == " " or s[1] == "\t"):
            p = s.split()
            n = len(p) - 1
            if n >= 3:
                i0 = _parse_face_v(p[1], vi)
                prev = _parse_face_v(p[2], vi)
                for j in range(3, n + 1):
                    cur = _parse_face_v(p[j], vi)
                    if 0 <= i0 < nv and 0 <= prev < nv and 0 <= cur < nv:
                        dx0 = qx[i0] * qs - cx; dy0 = qy[i0] * qs - cy; dz0 = qz[i0] * qs - cz
                        dx1 = qx[prev] * qs - cx; dy1 = qy[prev] * qs - cy; dz1 = qz[prev] * qs - cz
                        dx2 = qx[cur] * qs - cx; dy2 = qy[cur] * qs - cy; dz2 = qz[cur] * qs - cz
                        x0 = dx0 * rx + dy0 * ry + dz0 * rz; y0 = dx0 * ux + dy0 * uy + dz0 * uz; z0 = dx0 * fx + dy0 * fy + dz0 * fz
                        x1 = dx1 * rx + dy1 * ry + dz1 * rz; y1 = dx1 * ux + dy1 * uy + dz1 * uz; z1 = dx1 * fx + dy1 * fy + dz1 * fz
                        x2 = dx2 * rx + dy2 * ry + dz2 * rz; y2 = dx2 * ux + dy2 * uy + dz2 * uz; z2 = dx2 * fx + dy2 * fy + dz2 * fz
                        col = shade_and_cull_cam(x0, y0, z0, x1, y1, z1, x2, y2, z2, flip_n, wire)
                        if col >= 0:
                            pts = clip_and_project_tri(x0, y0, z0, x1, y1, z1, x2, y2, z2, near_z, focal)
                            if pts is not None:
                                batch.append((z0 + z1 + z2, col, pts))
                    prev = cur
                    if len(batch) >= STREAM_BATCH:
                        batch.sort(key=lambda it: -it[0])
                        for _, col_b, pts_b in batch:
                            if wire:
                                gint.dpoly(pts_b, gint.C_NONE, COL_WIRE)
                            else:
                                gint.dpoly(pts_b, col_b, gint.C_NONE)
                        batch.clear()
                        draw_hud(m, wire, flip_n, speed_tag, "STREAM...")
                        gint.dupdate()
                        if any_motion_key():
                            return 0
    if batch:
        batch.sort(key=lambda it: -it[0])
        for _, col_b, pts_b in batch:
            if wire:
                gint.dpoly(pts_b, gint.C_NONE, COL_WIRE)
            else:
                gint.dpoly(pts_b, col_b, gint.C_NONE)
    draw_hud(m, wire, flip_n, speed_tag)
    gint.dupdate()
    gc.collect()
    return -1


def reset_camera(m):
    return 0.0, 0.15 * m.radius, 2.65 * m.radius, 0.0, -0.06, 0.0, 210.0


def run_viewer():
    err = ""
    while True:
        raw_name = ask_obj_filename(err)
        if raw_name is None:
            return
        path = resolve_obj_path(raw_name)
        if path is None:
            err = "File not found: /" + raw_name + ".obj"
            continue
        model = load_obj_model(path)
        if model is None:
            err = "No valid v / f mesh in " + path
            continue

        cx, cy, cz, yaw, pitch, roll, focal = reset_camera(model)
        wire = False
        flip_n = 1.0
        slow_lock = False
        alpha_chord_used = False
        dirty = True
        need_full_stream = (model.mode != "ram")
        back_to_prompt = False
        last_step_ms = _now_ms()
        gint.clearevents()

        while not back_to_prompt:
            ev = gint.pollevent()
            while ev.type != gint.KEYEV_NONE:
                if ev.type == gint.KEYEV_DOWN:
                    k = ev.key
                    if k == gint.KEY_EXIT:
                        back_to_prompt = True
                        break
                    elif k == gint.KEY_ALPHA:
                        alpha_chord_used = False
                    elif k == gint.KEY_F1:
                        wire = not wire
                        dirty = True
                        need_full_stream = (model.mode != "ram")
                    elif k == gint.KEY_F2:
                        flip_n = -flip_n
                        dirty = True
                        need_full_stream = (model.mode != "ram")
                    elif k == gint.KEY_F3:
                        cx, cy, cz, yaw, pitch, roll, focal = reset_camera(model)
                        dirty = True
                        need_full_stream = (model.mode != "ram")
                    elif k == gint.KEY_F4:
                        new_name = ask_obj_filename("")
                        if new_name is not None:
                            np = resolve_obj_path(new_name)
                            if np is not None:
                                model = None
                                gc.collect()
                                nm = load_obj_model(np)
                                if nm is not None:
                                    model = nm
                                    cx, cy, cz, yaw, pitch, roll, focal = reset_camera(model)
                        gint.clearevents()
                        dirty = True
                        need_full_stream = (model.mode != "ram")
                elif ev.type == gint.KEYEV_UP and ev.key == gint.KEY_ALPHA:
                    if not alpha_chord_used:
                        slow_lock = not slow_lock
                        dirty = True
                ev = gint.pollevent()

            if back_to_prompt:
                break

            now_ms = _now_ms()
            dt = _diff_ms(now_ms, last_step_ms)
            if _IS_PC and dt < 16 and not dirty:
                time.sleep(0.004)
                continue
            last_step_ms = now_ms
            # Normalize step speed on fast PCs (~30Hz reference) vs calculator
            dt_scale = min(2.0, max(0.35, dt / 33.0)) if _IS_PC else 1.0

            alpha_held = gint.keydown(gint.KEY_ALPHA)
            shift_held = gint.keydown(gint.KEY_SHIFT)
            is_slow = alpha_held or slow_lock
            speed_tag = "SLW" if is_slow else ("FST" if shift_held else "")

            speed_mul = 3.0 if shift_held else (0.20 if is_slow else 1.0)
            turn_step = 0.065 * dt_scale  # keep normal camera rotation speed in [SLW]
            move_step = 0.065 * model.radius * speed_mul * dt_scale

            moved = False
            if gint.keydown(gint.KEY_RIGHT):
                yaw += turn_step; moved = True
            if gint.keydown(gint.KEY_LEFT):
                yaw -= turn_step; moved = True
            if gint.keydown(gint.KEY_UP):
                pitch = min(MAX_PITCH, pitch + turn_step); moved = True
            if gint.keydown(gint.KEY_DOWN):
                pitch = max(-MAX_PITCH, pitch - turn_step); moved = True
            if gint.keydown(gint.KEY_F5):
                roll -= turn_step; moved = True
            if gint.keydown(gint.KEY_F6):
                roll += turn_step; moved = True

            basis = compute_cam_basis(yaw, pitch, roll)
            rx, ry, rz, ux, uy, uz, fx, fy, fz = basis

            if gint.keydown(gint.KEY_8):
                cx += fx * move_step; cy += fy * move_step; cz += fz * move_step; moved = True
            if gint.keydown(gint.KEY_5):
                cx -= fx * move_step; cy -= fy * move_step; cz -= fz * move_step; moved = True
            if gint.keydown(gint.KEY_4):
                cx -= rx * move_step; cy -= ry * move_step; cz -= rz * move_step; moved = True
            if gint.keydown(gint.KEY_6):
                cx += rx * move_step; cy += ry * move_step; cz += rz * move_step; moved = True
            if gint.keydown(gint.KEY_1):
                cy += move_step; moved = True
            if gint.keydown(gint.KEY_0):
                cy -= move_step; moved = True

            zf = 1.0 + (0.018 if is_slow else (0.14 if shift_held else 0.07)) * dt_scale
            if gint.keydown(gint.KEY_ADD):
                focal = min(2800.0, focal * zf); moved = True
            if gint.keydown(gint.KEY_SUB):
                focal = max(25.0, focal / zf); moved = True

            if moved:
                if alpha_held:
                    alpha_chord_used = True
                dirty = True
                if model.mode != "ram":
                    need_full_stream = True

            if dirty:
                if model.mode == "ram":
                    render_ram_frame(model, cx, cy, cz, basis, wire, flip_n, focal, speed_tag)
                else:
                    render_preview_proxy(model, cx, cy, cz, basis, wire, flip_n, focal, speed_tag)
                dirty = False
            elif need_full_stream and not any_motion_key():
                rc = render_stream_full(model, cx, cy, cz, basis, wire, flip_n, focal, speed_tag)
                if rc == -1:
                    need_full_stream = False
            else:
                time.sleep(0.008)


run_viewer()
