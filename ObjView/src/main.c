/*
 * ObjView v2.1 Turbo -- High-Speed Artifact-Free 3D Wavefront .obj Viewer
 * Target: Casio fx-CG50 (OS 3.80 / 3.81 compatible via gint@dev)
 *
 * Fixes & upgrades in v2.1:
 *   - Horizon-locked (yaw, pitch, roll) 6-DOF free-fly camera (zero axis drift!)
 *   - Bright Valve Half-Lambert directional + hemisphere shading (no dark
 *     silhouettes; every wall angle in outdoor & indoor GMod maps is readable)
 *   - Precision Slow Step on ALPHA (hold ALPHA or tap ALPHA to toggle [SLW]:
 *     0.20x fine movement & zoom while keeping normal camera rotation speed;
 *     SHIFT = 3x fast boost)
 *   - Clean OS Main Menu exit (never returns from start() into Casio's
 *     broken add-in loader; EXIT returns to file prompt, EXIT/F6/MENU in
 *     file prompt opens Casio Main Menu via gint_osmenu() at stock clock)
 *   - Q8.20 single-pass vertex transform/projection + ILRAM Q16.16 Z-buffer
 *     rasterizer + 16-bucket front-to-back skybox occlusion + OPTN CPU toggle
 */
#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/gint.h>
#include <gint/kmalloc.h>
#include <gint/clock.h>
#include <gint/rtc.h>
#include <gint/defs/attributes.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define VIEW_TOP    16
#define VIEW_BOT    209
#define VIEW_H      (VIEW_BOT - VIEW_TOP + 1) /* 194 rows on 396x224 */
#define CX          (DWIDTH / 2)
#define CY          ((VIEW_TOP + VIEW_BOT) / 2)

#define MAX_RAM_V   12000
#define MAX_RAM_T   18000
#define WIN_V       8192
#define OVERLAP_V   1536
#define CHUNK_SZ    4096

/* Q8.20 fixed-point unit: model.radius == 1,048,576 */
#define FP_SHIFT    20
#define FP_RAD      (1 << FP_SHIFT)
#define NEAR_FP     26214   /* 0.025 * radius in Q8.20 */
#define GUARD_XY    16384
#define MAX_PITCH   1.553343f /* ~89 deg: prevents accidental upside-down flip */

/* Frustum outcode flags */
#define OUT_NEAR    0x01
#define OUT_LEFT    0x02
#define OUT_RIGHT   0x04
#define OUT_TOP     0x08
#define OUT_BOT     0x10
#define OUT_GUARD   0x20

typedef struct { int32_t x, y, z; } vfp3_t;

typedef struct {
    uint16_t a, b, c;
    int8_t nx, ny, nz;
    uint8_t flags;
} tri_t;

typedef struct {
    int32_t vx, vy, vz;
    int16_t sx, sy;
    uint16_t sz;
    uint8_t outcode;
    uint8_t pad;
} pvert_t;

typedef struct {
    uint16_t tri_idx;
    uint16_t col;
    uint16_t next;
} vis_node_t;

typedef struct {
    char path[96];
    int n_verts;
    int n_tris;
    float cx, cy, cz;
    float radius;
    float inv_rad_fp;
    int stream_only;
    vfp3_t *verts;
    tri_t  *tris;
} obj_model_t;

static uint16_t gray_pal[32];
static uint16_t *zbuf = NULL;
static uint16_t recip_q16[512];

/* Saved stock clock registers */
static struct cpg_overclock_setting saved_clk;
static int clk_mode = 0; /* 0 = 232M (F4), 1 = 189B (F5), 2 = 116M (F1) */
static const int clk_presets[3] = { CLOCK_SPEED_F4, CLOCK_SPEED_F5, CLOCK_SPEED_F1 };
static const char *clk_names[3] = { "232M", "189B", "116M" };

static void apply_active_clock(void)
{
    clock_set_speed(clk_presets[clk_mode]);
}

static void restore_stock_clock(void)
{
    cpg_set_overclock_setting(&saved_clk);
}

/* Safe OS-world file wrappers */
static int os_open_ro(const char *path)
{
    return (int)gint_world_switch(GINT_CALL(open, path, O_RDONLY, 0644));
}
static int os_read_chunk(int fd, char *buf, int sz)
{
    return (int)gint_world_switch(GINT_CALL(read, fd, buf, (size_t)sz));
}
static void os_close_fd(int fd)
{
    (void)gint_world_switch(GINT_CALL(close, fd));
}

typedef struct {
    int fd;
    char buf[CHUNK_SZ];
    int pos, len, eof;
} line_reader_t;

/* Keep 4 KB I/O buffer + 384 B line buffer in static .bss off the 16 KB stack! */
static line_reader_t g_lr;
static char g_line[384];

static void lr_init(line_reader_t *lr, int fd)
{
    lr->fd = fd;
    lr->pos = 0;
    lr->len = 0;
    lr->eof = 0;
}

static int lr_getline(line_reader_t *lr, char *out, int max_out)
{
    int n = 0;
    while (1) {
        if (lr->pos >= lr->len) {
            if (lr->eof) return (n > 0) ? 1 : 0;
            int r = os_read_chunk(lr->fd, lr->buf, CHUNK_SZ);
            if (r <= 0) {
                lr->eof = 1;
                out[n] = '\0';
                return (n > 0) ? 1 : 0;
            }
            lr->pos = 0;
            lr->len = r;
        }
        char c = lr->buf[lr->pos++];
        if (c == '\r') continue;
        if (c == '\n') {
            if (n > 0 && out[n - 1] == '\\') {
                out[n - 1] = ' ';
                continue;
            }
            out[n] = '\0';
            return 1;
        }
        if (n < max_out - 1) out[n++] = c;
    }
}

static const char *skip_ws(const char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

static const char *parse_float(const char *s, float *out)
{
    s = skip_ws(s);
    if (!*s) return s;
    float sign = 1.0f;
    if (*s == '-') { sign = -1.0f; s++; }
    else if (*s == '+') s++;
    double val = 0.0;
    while (*s >= '0' && *s <= '9') { val = val * 10.0 + (*s - '0'); s++; }
    if (*s == '.') {
        s++;
        double frac = 0.1;
        while (*s >= '0' && *s <= '9') {
            val += (*s - '0') * frac;
            frac *= 0.1;
            s++;
        }
    }
    if (*s == 'e' || *s == 'E') {
        s++;
        int esign = 1, expv = 0;
        if (*s == '-') { esign = -1; s++; }
        else if (*s == '+') s++;
        while (*s >= '0' && *s <= '9') { expv = expv * 10 + (*s - '0'); s++; }
        double p10 = 1.0;
        while (expv > 0) { p10 *= 10.0; expv--; }
        val = (esign > 0) ? (val * p10) : (val / p10);
    }
    *out = (float)(val * sign);
    return s;
}

static const char *parse_face_v(const char *s, int n_verts_so_far, int *v_idx)
{
    s = skip_ws(s);
    if (!*s || *s == '#') return NULL;
    int sign = 1;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    if (*s < '0' || *s > '9') return NULL;
    int v = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    while (*s && *s != ' ' && *s != '\t') s++;
    v *= sign;
    *v_idx = (v > 0) ? (v - 1) : (n_verts_so_far + v);
    return s;
}

static void draw_banner(const char *title, const char *sub)
{
    dclear(gray_pal[29]);
    drect_border(34, 72, DWIDTH - 35, 148, C_WHITE, 2, C_BLACK);
    drect(34, 72, DWIDTH - 35, 92, C_BLACK);
    dtext(44, 77, C_WHITE, title);
    if (sub) dtext(44, 108, gray_pal[5], sub);
    dupdate();
}

static void free_model(obj_model_t *m)
{
    if (m->verts) { free(m->verts); m->verts = NULL; }
    if (m->tris)  { free(m->tris);  m->tris = NULL;  }
}

static int resolve_obj(const char *raw, char *out, int max_len)
{
    char clean[64];
    while (*raw == ' ' || *raw == '/' || *raw == '\\') raw++;
    strncpy(clean, raw, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0';
    int n = (int)strlen(clean);
    if (n > 4 && (strcmp(clean + n - 4, ".obj") == 0 || strcmp(clean + n - 4, ".OBJ") == 0))
        clean[n - 4] = '\0';

    const char *fmts[4] = { "/%s.obj", "/%s.OBJ", "%s.obj", "/%s" };
    restore_stock_clock();
    for (int i = 0; i < 4; i++) {
        snprintf(out, (size_t)max_len, fmts[i], clean);
        int fd = os_open_ro(out);
        if (fd >= 0) {
            os_close_fd(fd);
            apply_active_clock();
            return 1;
        }
    }
    apply_active_clock();
    return 0;
}

static int load_model(const char *path, obj_model_t *m)
{
    restore_stock_clock();
    int fd = os_open_ro(path);
    if (fd < 0) { apply_active_clock(); return -1; }

    memset(m, 0, sizeof(*m));
    strncpy(m->path, path, sizeof(m->path) - 1);
    draw_banner("Loading .obj...", path);

    lr_init(&g_lr, fd);
    float minx = 1e30f, miny = 1e30f, minz = 1e30f;
    float maxx = -1e30f, maxy = -1e30f, maxz = -1e30f;
    int nv = 0, nt = 0;

    while (lr_getline(&g_lr, g_line, sizeof(g_line))) {
        const char *s = skip_ws(g_line);
        if (s[0] == 'v' && (s[1] == ' ' || s[1] == '\t')) {
            float x = 0, y = 0, z = 0;
            s = parse_float(s + 2, &x);
            s = parse_float(s, &y);
            s = parse_float(s, &z);
            if (x < minx) minx = x;
            if (x > maxx) maxx = x;
            if (y < miny) miny = y;
            if (y > maxy) maxy = y;
            if (z < minz) minz = z;
            if (z > maxz) maxz = z;
            nv++;
        } else if (s[0] == 'f' && (s[1] == ' ' || s[1] == '\t')) {
            const char *p = s + 2;
            int idx, k = 0;
            while ((p = parse_face_v(p, nv, &idx)) != NULL) k++;
            if (k >= 3) nt += (k - 2);
        }
    }
    os_close_fd(fd);

    if (nv <= 0 || nt <= 0) { apply_active_clock(); return -2; }

    m->n_verts = nv;
    m->n_tris = nt;
    m->cx = 0.5f * (minx + maxx);
    m->cy = 0.5f * (miny + maxy);
    m->cz = 0.5f * (minz + maxz);
    float dx = maxx - minx, dy = maxy - miny, dz = maxz - minz;
    m->radius = 0.5f * sqrtf(dx * dx + dy * dy + dz * dz);
    if (m->radius < 1e-5f) m->radius = 1.0f;
    m->inv_rad_fp = (float)FP_RAD / m->radius;

    if (nv <= MAX_RAM_V && nt <= MAX_RAM_T) {
        m->verts = malloc((size_t)nv * sizeof(vfp3_t));
        m->tris  = malloc((size_t)nt * sizeof(tri_t));
        if (m->verts && m->tris) {
            fd = os_open_ro(path);
            if (fd >= 0) {
                lr_init(&g_lr, fd);
                int vi = 0, ti = 0;
                const float sfp = m->inv_rad_fp;
                while (lr_getline(&g_lr, g_line, sizeof(g_line))) {
                    const char *s = skip_ws(g_line);
                    if (s[0] == 'v' && (s[1] == ' ' || s[1] == '\t') && vi < nv) {
                        float x = 0, y = 0, z = 0;
                        s = parse_float(s + 2, &x);
                        s = parse_float(s, &y);
                        s = parse_float(s, &z);
                        m->verts[vi].x = (int32_t)((x - m->cx) * sfp);
                        m->verts[vi].y = (int32_t)((y - m->cy) * sfp);
                        m->verts[vi].z = (int32_t)((z - m->cz) * sfp);
                        vi++;
                    } else if (s[0] == 'f' && (s[1] == ' ' || s[1] == '\t')) {
                        const char *p = s + 2;
                        int i0, prev, cur;
                        p = parse_face_v(p, vi, &i0);
                        if (!p) continue;
                        p = parse_face_v(p, vi, &prev);
                        if (!p) continue;
                        while ((p = parse_face_v(p, vi, &cur)) != NULL) {
                            if (i0 >= 0 && i0 < vi && prev >= 0 && prev < vi &&
                                cur >= 0 && cur < vi && ti < nt) {
                                m->tris[ti].a = (uint16_t)i0;
                                m->tris[ti].b = (uint16_t)prev;
                                m->tris[ti].c = (uint16_t)cur;
                                /* Precompute right-handed world unit face normal */
                                float e1x = (float)(m->verts[prev].x - m->verts[i0].x);
                                float e1y = (float)(m->verts[prev].y - m->verts[i0].y);
                                float e1z = (float)(m->verts[prev].z - m->verts[i0].z);
                                float e2x = (float)(m->verts[cur].x  - m->verts[i0].x);
                                float e2y = (float)(m->verts[cur].y  - m->verts[i0].y);
                                float e2z = (float)(m->verts[cur].z  - m->verts[i0].z);
                                float nx = e1y * e2z - e1z * e2y;
                                float ny = e1z * e2x - e1x * e2z;
                                float nz = e1x * e2y - e1y * e2x;
                                float nl = sqrtf(nx * nx + ny * ny + nz * nz);
                                if (nl > 1e-6f) {
                                    float inv = 127.0f / nl;
                                    m->tris[ti].nx = (int8_t)(nx * inv);
                                    m->tris[ti].ny = (int8_t)(ny * inv);
                                    m->tris[ti].nz = (int8_t)(nz * inv);
                                    m->tris[ti].flags = 1;
                                } else {
                                    m->tris[ti].nx = 0;
                                    m->tris[ti].ny = 0;
                                    m->tris[ti].nz = 0;
                                    m->tris[ti].flags = 0;
                                }
                                ti++;
                            }
                            prev = cur;
                        }
                    }
                }
                os_close_fd(fd);
                m->n_tris = ti;
                m->stream_only = 0;
                apply_active_clock();
                return 0;
            }
        }
        free_model(m);
    }

    m->stream_only = 1;
    apply_active_clock();
    return 0;
}

/*
 * Hot Z-buffered flat-shaded triangle rasterizer in 0-wait-state CPU ILRAM!
 */
GILRAM static void draw_tri_zbuf_fp(int x0, int y0, uint16_t z0,
                                    int x1, int y1, uint16_t z1,
                                    int x2, int y2, uint16_t z2,
                                    uint16_t col)
{
    int t;
    uint16_t tz;
    if (y0 > y1) { t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; tz = z0; z0 = z1; z1 = tz; }
    if (y1 > y2) { t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t; tz = z1; z1 = z2; z2 = tz; }
    if (y0 > y1) { t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; tz = z0; z0 = z1; z1 = tz; }

    if (y2 < VIEW_TOP || y0 > VIEW_BOT || y0 == y2) return;

    int total_h = y2 - y0;
    int32_t dxl = (int32_t)(((int64_t)(x2 - x0) * 65536LL) / total_h);
    int32_t dzl = (int32_t)(((int64_t)((int)z2 - (int)z0) * 32768LL) / total_h);

    uint16_t * const vram_base = gint_vram;
    uint16_t * const zbuf_base = zbuf;

    for (int part = 0; part < 2; part++) {
        int ys = (part == 0) ? y0 : y1;
        int ye = (part == 0) ? y1 : y2;
        int seg_h = ye - ys;
        if (seg_h <= 0) continue;

        int y_end = (part == 0 && y1 < y2) ? (y1 - 1) : ye;
        int y_start = ys;
        if (y_start < VIEW_TOP) y_start = VIEW_TOP;
        if (y_end   > VIEW_BOT) y_end   = VIEW_BOT;
        if (y_start > y_end) continue;

        int xs_base = (part == 0) ? x0 : x1;
        int zs_base = (part == 0) ? (int)z0 : (int)z1;
        int xe_targ = (part == 0) ? x1 : x2;
        int ze_targ = (part == 0) ? (int)z1 : (int)z2;

        int32_t dxs = (int32_t)(((int64_t)(xe_targ - xs_base) * 65536LL) / seg_h);
        int32_t dzs = (int32_t)(((int64_t)(ze_targ - zs_base) * 32768LL) / seg_h);

        int step_l = y_start - y0;
        int step_s = y_start - ys;
        int64_t xl_fp = (int64_t)x0 * 65536LL + (int64_t)dxl * step_l;
        int64_t zl_fp = (int64_t)z0 * 32768LL + (int64_t)dzl * step_l;
        int64_t xs_fp = (int64_t)xs_base * 65536LL + (int64_t)dxs * step_s;
        int64_t zs_fp = (int64_t)zs_base * 32768LL + (int64_t)dzs * step_s;

        for (int y = y_start; y <= y_end; y++) {
            int xa = (int)(xl_fp >> 16);
            int xb = (int)(xs_fp >> 16);
            int32_t za = (int32_t)zl_fp;
            int32_t zb = (int32_t)zs_fp;

            xl_fp += dxl; zl_fp += dzl;
            xs_fp += dxs; zs_fp += dzs;

            if (xa > xb) {
                int tx = xa; xa = xb; xb = tx;
                int32_t tzp = za; za = zb; zb = tzp;
            }
            if (xb < 0 || xa >= DWIDTH) continue;

            int span = xb - xa;
            int32_t dz_span;
            if (span <= 0) {
                dz_span = 0;
            } else if (span < 512) {
                dz_span = (int32_t)(((int64_t)(zb - za) * recip_q16[span]) >> 16);
            } else {
                dz_span = (zb - za) / span;
            }

            int32_t cz = za;
            if (xa < 0) {
                cz += (int32_t)((int64_t)dz_span * (-xa));
                xa = 0;
            }
            if (xb >= DWIDTH) xb = DWIDTH - 1;

            uint16_t *zrow = zbuf_base + (y - VIEW_TOP) * DWIDTH;
            uint16_t *vrow = vram_base + y * DWIDTH;

            for (int x = xa; x <= xb; x++) {
                int iz = cz >> 15;
                if ((unsigned)iz > 65535u) iz = (iz < 0) ? 0 : 65535;
                if ((uint16_t)iz > zrow[x]) {
                    zrow[x] = (uint16_t)iz;
                    vrow[x] = col;
                }
                cz += dz_span;
            }
        }
    }
}

/* Clipped wireframe segment helper */
static void draw_wire_seg(int x0, int y0, int x1, int y1, uint16_t col)
{
    for (int iter = 0; iter < 8; iter++) {
        int c0 = (x0 < 0 ? 1 : 0) | (x0 >= DWIDTH ? 2 : 0) |
                 (y0 < VIEW_TOP ? 4 : 0) | (y0 > VIEW_BOT ? 8 : 0);
        int c1 = (x1 < 0 ? 1 : 0) | (x1 >= DWIDTH ? 2 : 0) |
                 (y1 < VIEW_TOP ? 4 : 0) | (y1 > VIEW_BOT ? 8 : 0);
        if ((c0 | c1) == 0) {
            dline(x0, y0, x1, y1, col);
            return;
        }
        if ((c0 & c1) != 0) return;
        int co = c0 ? c0 : c1;
        int nx = 0, ny = 0;
        if (co & 8) {
            nx = x0 + (int)(((int64_t)(x1 - x0) * (VIEW_BOT - y0)) / (y1 - y0));
            ny = VIEW_BOT;
        } else if (co & 4) {
            nx = x0 + (int)(((int64_t)(x1 - x0) * (VIEW_TOP - y0)) / (y1 - y0));
            ny = VIEW_TOP;
        } else if (co & 2) {
            ny = y0 + (int)(((int64_t)(y1 - y0) * (DWIDTH - 1 - x0)) / (x1 - x0));
            nx = DWIDTH - 1;
        } else {
            ny = y0 + (int)(((int64_t)(y1 - y0) * (0 - x0)) / (x1 - x0));
            nx = 0;
        }
        if (co == c0) { x0 = nx; y0 = ny; }
        else          { x1 = nx; y1 = ny; }
    }
}

/*
 * Analytical 3D near-plane & guardband clipper (seamless Half-Lambert match!)
 */
static void render_tri_clipped_fp(vfp3_t p0, vfp3_t p1, vfp3_t p2,
                                  int32_t focal_i, int flip_sign,
                                  uint16_t pre_col, int has_pre_col, int wire)
{
    vfp3_t in_v[4] = { p0, p1, p2, {0,0,0} };
    vfp3_t out_v[4];

    if (!wire) {
        int64_t e1x = (p1.x - p0.x) / 16, e1y = (p1.y - p0.y) / 16, e1z = (p1.z - p0.z) / 16;
        int64_t e2x = (p2.x - p0.x) / 16, e2y = (p2.y - p0.y) / 16, e2z = (p2.z - p0.z) / 16;
        int64_t nx = e1y * e2z - e1z * e2y;
        int64_t ny = e1z * e2x - e1x * e2z;
        int64_t nz = e1x * e2y - e1y * e2x;
        int64_t facing = (nx * (p0.x / 16) + ny * (p0.y / 16) + nz * (p0.z / 16)) * flip_sign;
        if (facing <= 0) return;
        if (!has_pre_col) {
            float fnx = (float)nx, fny = (float)ny, fnz = (float)nz;
            float nlen = sqrtf(fnx * fnx + fny * fny + fnz * fnz);
            if (nlen < 1e-6f) return;
            /* Camera space is left-handed (det=-1), so outward normal is (-Nc/nlen)*flip_sign */
            float s = -(float)flip_sign / nlen;
            float dot_l = (fnx * s) * 0.35f + (fny * s) * 0.50f - (fnz * s) * 0.79f;
            float h = 0.5f * (dot_l + 1.0f);
            if (h < 0.0f) h = 0.0f;
            if (h > 1.0f) h = 1.0f;
            int lvl = 6 + (int)(h * 21.0f);
            if (lvl > 27) lvl = 27;
            pre_col = gray_pal[lvl];
        }
    } else {
        pre_col = gray_pal[3];
    }

    int n_out = 0;
    vfp3_t prev = in_v[2];
    int prev_in = (prev.z >= NEAR_FP);
    for (int i = 0; i < 3; i++) {
        vfp3_t cur = in_v[i];
        int cur_in = (cur.z >= NEAR_FP);
        if (cur_in != prev_in) {
            int32_t dz = cur.z - prev.z;
            int32_t num = NEAR_FP - prev.z;
            vfp3_t iv;
            iv.x = prev.x + (int32_t)(((int64_t)(cur.x - prev.x) * num) / dz);
            iv.y = prev.y + (int32_t)(((int64_t)(cur.y - prev.y) * num) / dz);
            iv.z = NEAR_FP;
            out_v[n_out++] = iv;
        }
        if (cur_in) out_v[n_out++] = cur;
        prev = cur;
        prev_in = cur_in;
    }
    if (n_out < 3) return;

    int sx[4], sy[4];
    uint16_t sz[4];
    for (int i = 0; i < n_out; i++) {
        int32_t vz = out_v[i].z;
        int64_t px = CX + ((int64_t)out_v[i].x * focal_i) / vz;
        int64_t py = CY - ((int64_t)out_v[i].y * focal_i) / vz;
        if (px < -262144) px = -262144;
        if (px >  262144) px =  262144;
        if (py < -262144) py = -262144;
        if (py >  262144) py =  262144;
        sx[i] = (int)px;
        sy[i] = (int)py;
        int iz = (int)(((int64_t)NEAR_FP * 65534LL) / vz) + 1;
        if (iz < 1) iz = 1;
        if (iz > 65535) iz = 65535;
        sz[i] = (uint16_t)iz;
    }

    if (wire || !zbuf) {
        for (int i = 0; i < n_out; i++) {
            int j = (i + 1 == n_out) ? 0 : (i + 1);
            draw_wire_seg(sx[i], sy[i], sx[j], sy[j], pre_col);
        }
    } else {
        draw_tri_zbuf_fp(sx[0], sy[0], sz[0], sx[1], sy[1], sz[1], sx[2], sy[2], sz[2], pre_col);
        if (n_out == 4)
            draw_tri_zbuf_fp(sx[0], sy[0], sz[0], sx[2], sy[2], sz[2], sx[3], sy[3], sz[3], pre_col);
    }
}

static void clear_viewport_and_zbuf(uint16_t bg_col, int clear_z)
{
    uint32_t bg32 = ((uint32_t)bg_col << 16) | bg_col;
    uint32_t *vp = (uint32_t *)(gint_vram + VIEW_TOP * DWIDTH);
    int n32 = (DWIDTH * VIEW_H) / 2;
    for (int i = 0; i < n32; i++) vp[i] = bg32;

    if (clear_z && zbuf) {
        uint32_t *zp = (uint32_t *)zbuf;
        for (int i = 0; i < n32; i++) zp[i] = 0;
    }
}

/* Filename input dialog */
static int key_letter(int key)
{
    switch (key) {
    case KEY_XOT: return 'a'; case KEY_LOG: return 'b'; case KEY_LN: return 'c';
    case KEY_SIN: return 'd'; case KEY_COS: return 'e'; case KEY_TAN: return 'f';
    case KEY_FRAC: return 'g'; case KEY_FD: return 'h'; case KEY_LEFTP: return 'i';
    case KEY_RIGHTP: return 'j'; case KEY_COMMA: return 'k'; case KEY_ARROW: return 'l';
    case KEY_7: return 'm'; case KEY_8: return 'n'; case KEY_9: return 'o';
    case KEY_4: return 'p'; case KEY_5: return 'q'; case KEY_6: return 'r';
    case KEY_MUL: return 's'; case KEY_DIV: return 't';
    case KEY_1: return 'u'; case KEY_2: return 'v'; case KEY_3: return 'w';
    case KEY_ADD: return 'x'; case KEY_SUB: return 'y'; case KEY_0: return 'z';
    default: return 0;
    }
}

static int key_digit_sym(int key)
{
    switch (key) {
    case KEY_0: return '0'; case KEY_1: return '1'; case KEY_2: return '2';
    case KEY_3: return '3'; case KEY_4: return '4'; case KEY_5: return '5';
    case KEY_6: return '6'; case KEY_7: return '7'; case KEY_8: return '8';
    case KEY_9: return '9'; case KEY_DOT: return '.'; case KEY_SUB: return '-';
    case KEY_NEG: return '_'; case KEY_DIV: return '/';
    default: return 0;
    }
}

/*
 * File prompt:
 *   allow_cancel == 1 (opened via F4 from viewer) -> EXIT/F6 returns 0 to viewer
 *   allow_cancel == 0 (main file screen)          -> EXIT/F6/MENU opens OS Main Menu!
 */
static int prompt_filename(char *out_name, int max_len, const char *err_msg, int allow_cancel)
{
    char buf[64] = "";
    int mode = 0; /* 0=abc, 1=ABC, 2=123 */
    const char *mnames[3] = { "abc", "ABC", "123" };
    clearevents();

    while (1) {
        char line[96];
        dclear(gray_pal[29]);
        drect(0, 0, DWIDTH - 1, 16, C_BLACK);
        dtext(6, 3, C_WHITE, "ObjView 2.21 Turbo (.obj)");
        drect(DWIDTH - 42, 2, DWIDTH - 5, 14, C_RGB(4, 12, 26));
        dtext(DWIDTH - 36, 3, C_WHITE, mnames[mode]);

        dtext(18, 36, gray_pal[4], "Enter .obj file name (without .obj):");
        drect_border(18, 56, DWIDTH - 19, 82, C_WHITE, 1, C_BLACK);
        snprintf(line, sizeof(line), "%s_", buf);
        dtext(24, 64, C_BLACK, line);
        snprintf(line, sizeof(line), "Target: /%s.obj", buf[0] ? buf : "<name>");
        dtext(24, 88, gray_pal[12], line);

        if (err_msg && err_msg[0])
            dtext(18, 112, C_RED, err_msg);

        dtext(18, 138, gray_pal[9], "EXE: Open  DEL: Erase  ALPHA/F3: abc/ABC/123");
        dtext(18, 154, gray_pal[9], "F4: '_'    F5: '/'     MENU/EXIT: Main Menu");

        const char *fk[6] = { "OPEN", "", "a/A/1", "_", "/", allow_cancel ? "BACK" : "MENU" };
        drect(0, DHEIGHT - 18, DWIDTH - 1, DHEIGHT - 1, C_WHITE);
        dhline(DHEIGHT - 19, gray_pal[18]);
        for (int i = 0; i < 6; i++) {
            if (!fk[i][0]) continue;
            int x1 = i * 64 + 8, x2 = (i + 1) * 64 + 3;
            drect_border(x1, DHEIGHT - 16, x2, DHEIGHT - 2,
                         i == 0 ? C_BLACK : C_WHITE, 1, C_BLACK);
            dtext_opt((x1 + x2) / 2, DHEIGHT - 9,
                      i == 0 ? C_WHITE : C_BLACK, C_NONE,
                      DTEXT_CENTER, DTEXT_MIDDLE, fk[i], -1);
        }
        dupdate();

        restore_stock_clock();
        key_event_t ev = getkey_opt(GETKEY_MOD_SHIFT | GETKEY_MOD_ALPHA, NULL);
        apply_active_clock();
        int k = ev.key;
        int len = (int)strlen(buf);

        if (k == KEY_MENU) {
            return -1; /* Clean exit via gint_setrestart(1) -> kquit() -> gint_osmenu_native() */
        }
        if (k == KEY_EXIT || k == KEY_F6) {
            if (allow_cancel) return 0;
            return -1; /* Clean exit to OS Main Menu */
        }
        if (k == KEY_EXE || k == KEY_F1) {
            if (len > 0) {
                strncpy(out_name, buf, (size_t)(max_len - 1));
                out_name[max_len - 1] = '\0';
                return 1;
            }
            err_msg = "Please type a file name first";
            continue;
        }
        if (k == KEY_DEL) {
            if (len > 0) buf[len - 1] = '\0';
            continue;
        }
        if (k == KEY_ALPHA || k == KEY_F3) {
            mode = (mode + 1) % 3;
            continue;
        }
        if (k == KEY_F4 && len < 56) { buf[len] = '_'; buf[len + 1] = '\0'; continue; }
        if (k == KEY_F5 && len < 56) { buf[len] = '/'; buf[len + 1] = '\0'; continue; }

        int ch = 0;
        if (mode == 2) {
            ch = key_digit_sym(k);
        } else {
            ch = key_letter(k);
            if (ch && (mode == 1 || ev.shift)) ch = ch - 'a' + 'A';
            else if (!ch && ev.alpha) ch = key_digit_sym(k);
        }
        if (ch && len < 56) {
            buf[len] = (char)ch;
            buf[len + 1] = '\0';
            err_msg = "";
        }
    }
}

/*
 * Horizon-locked Garry's Mod / Source Engine 6-DOF camera basis:
 *   yaw   : horizontal turn around world +Y (horizon stays 100% level!)
 *   pitch : vertical look up/down (clamped to +-89 deg)
 *   roll  : optional optical roll around forward axis F (F5 / F6)
 */
static void compute_cam_basis(float yaw, float pitch, float roll,
                              float *rx, float *ry, float *rz,
                              float *ux, float *uy, float *uz,
                              float *fx, float *fy, float *fz)
{
    float cy = cosf(yaw),   sy = sinf(yaw);
    float cp = cosf(pitch), sp = sinf(pitch);
    float cr = cosf(roll),  sr = sinf(roll);

    *fx =  sy * cp;
    *fy =  sp;
    *fz = -cy * cp;

    float r0x =  cy,      r0y = 0.0f, r0z = sy;
    float u0x = -sy * sp, u0y = cp,   u0z = cy * sp;

    *rx =  r0x * cr + u0x * sr;
    *ry =  r0y * cr + u0y * sr;
    *rz =  r0z * cr + u0z * sr;

    *ux = -r0x * sr + u0x * cr;
    *uy = -r0y * sr + u0y * cr;
    *uz = -r0z * sr + u0z * cr;
}

int main(void)
{
    /* Never return to Casio's broken OS add-in loader; use OS menu switching */
    gint_setrestart(1);

    cpg_get_overclock_setting(&saved_clk);
    cpg_set_overclock_permanent(false);
    apply_active_clock();

    for (int i = 0; i < 32; i++) gray_pal[i] = C_RGB(i, i, i);
    recip_q16[0] = 65535;
    for (int i = 1; i < 512; i++) {
        uint32_t r = (1u << 16) / (uint32_t)i;
        recip_q16[i] = (r > 65535u) ? 65535u : (uint16_t)r;
    }

    zbuf = kmalloc(DWIDTH * VIEW_H * sizeof(uint16_t), "_uram");
    if (!zbuf) zbuf = malloc(DWIDTH * VIEW_H * sizeof(uint16_t));

    obj_model_t model = { 0 };
    char raw_name[64], full_path[96], err[72] = "";

    /* Outer session loop:
     *   EXIT inside 3D view -> returns to file prompt.
     *   MENU anywhere, or EXIT/F6 on file prompt -> restores stock clock,
     *   frees buffers, and returns 0 from main(). With gint_setrestart(1),
     *   gint's start() runs kquit() + gint_osmenu_native() directly in the
     *   OS world WITHOUT gint_world_switch(), avoiding the 0x1080 stack-canary
     *   panic when re-entering the add-in on OS 3.81! */
    for (;;) {
        err[0] = '\0';
        while (1) {
            int pr = prompt_filename(raw_name, sizeof(raw_name), err, 0);
            if (pr < 0) {
                if (zbuf) { free(zbuf); zbuf = NULL; }
                restore_stock_clock();
                return 0;
            }
            if (!resolve_obj(raw_name, full_path, sizeof(full_path))) {
                snprintf(err, sizeof(err), "Not found: /%s.obj", raw_name);
                continue;
            }
            if (load_model(full_path, &model) < 0) {
                snprintf(err, sizeof(err), "Invalid or empty .obj mesh");
                continue;
            }
            break;
        }

        pvert_t *pverts = NULL;
        vis_node_t *vis_nodes = NULL;
        if (!model.stream_only) {
            pverts    = malloc((size_t)model.n_verts * sizeof(pvert_t));
            vis_nodes = malloc((size_t)model.n_tris  * sizeof(vis_node_t));
        }

        /* Camera state in normalized units (radius == 1.0f) */
        float cx = 0.0f, cy = 0.15f, cz = 2.65f;
        float yaw = 0.0f, pitch = -0.06f, roll = 0.0f;
        float focal = 210.0f;
        int flip_sign = 1;
        int wire = 0;
        int slow_lock = 0;
        int alpha_chord_used = 0;
        int dirty = 1;
        int last_ms = 0;
        int back_to_prompt = 0;
        int quit_to_menu = 0;

        clearevents();

        while (!back_to_prompt && !quit_to_menu) {
            key_event_t ev;
            while ((ev = pollevent()).type != KEYEV_NONE) {
                if (ev.type == KEYEV_DOWN) {
                    if (ev.key == KEY_EXIT) {
                        back_to_prompt = 1;
                        break;
                    }
                    if (ev.key == KEY_MENU) {
                        quit_to_menu = 1;
                        break;
                    }
                    if (ev.key == KEY_OPTN) {
                        clk_mode = (clk_mode + 1) % 3;
                        apply_active_clock();
                        dirty = 1;
                    }
                    if (ev.key == KEY_ALPHA) {
                        alpha_chord_used = 0;
                    }
                    if (ev.key == KEY_F1) { wire = !wire; dirty = 1; }
                    if (ev.key == KEY_F2) { flip_sign = -flip_sign; dirty = 1; }
                    if (ev.key == KEY_F3) {
                        cx = 0.0f; cy = 0.15f; cz = 2.65f;
                        yaw = 0.0f; pitch = -0.06f; roll = 0.0f;
                        focal = 210.0f;
                        dirty = 1;
                    }
                    if (ev.key == KEY_F4) {
                        int pr = prompt_filename(raw_name, sizeof(raw_name), "", 1);
                        if (pr < 0) {
                            quit_to_menu = 1;
                            break;
                        }
                        if (pr > 0) {
                            if (resolve_obj(raw_name, full_path, sizeof(full_path))) {
                                if (vis_nodes) { free(vis_nodes); vis_nodes = NULL; }
                                if (pverts)    { free(pverts);    pverts = NULL;    }
                                free_model(&model);
                                if (load_model(full_path, &model) == 0) {
                                    if (!model.stream_only) {
                                        pverts    = malloc((size_t)model.n_verts * sizeof(pvert_t));
                                        vis_nodes = malloc((size_t)model.n_tris  * sizeof(vis_node_t));
                                    }
                                    cx = 0.0f; cy = 0.15f; cz = 2.65f;
                                    yaw = 0.0f; pitch = -0.06f; roll = 0.0f;
                                    focal = 210.0f;
                                }
                            }
                        }
                        clearevents();
                        dirty = 1;
                    }
                } else if (ev.type == KEYEV_UP && ev.key == KEY_ALPHA) {
                    /* Tapping ALPHA alone toggles precision slow-step lock! */
                    if (!alpha_chord_used) {
                        slow_lock = !slow_lock;
                        dirty = 1;
                    }
                }
            }
            if (back_to_prompt || quit_to_menu) break;

            int alpha_held = keydown(KEY_ALPHA);
            int shift_held = keydown(KEY_SHIFT);
            int is_slow = alpha_held || slow_lock;

            float speed_mul = 1.0f;
            if (shift_held)      speed_mul = 3.0f;
            else if (is_slow)    speed_mul = 0.20f;

            /* Keep camera rotation speed unchanged in [SLW] mode */
            float turn_step = 0.065f;
            float move_step = 0.065f * speed_mul;

            int moved = 0;
            if (keydown(KEY_RIGHT)) { yaw   += turn_step; moved = 1; }
            if (keydown(KEY_LEFT))  { yaw   -= turn_step; moved = 1; }
            if (keydown(KEY_UP))    { pitch += turn_step; if (pitch >  MAX_PITCH) pitch =  MAX_PITCH; moved = 1; }
            if (keydown(KEY_DOWN))  { pitch -= turn_step; if (pitch < -MAX_PITCH) pitch = -MAX_PITCH; moved = 1; }
            if (keydown(KEY_F5))    { roll  -= turn_step; moved = 1; }
            if (keydown(KEY_F6))    { roll  += turn_step; moved = 1; }

            float rx, ry, rz, ux, uy, uz, fx, fy, fz;
            compute_cam_basis(yaw, pitch, roll, &rx, &ry, &rz, &ux, &uy, &uz, &fx, &fy, &fz);

            if (keydown(KEY_8)) { cx += fx * move_step; cy += fy * move_step; cz += fz * move_step; moved = 1; }
            if (keydown(KEY_5)) { cx -= fx * move_step; cy -= fy * move_step; cz -= fz * move_step; moved = 1; }
            if (keydown(KEY_4)) { cx -= rx * move_step; cy -= ry * move_step; cz -= rz * move_step; moved = 1; }
            if (keydown(KEY_6)) { cx += rx * move_step; cy += ry * move_step; cz += rz * move_step; moved = 1; }
            if (keydown(KEY_1)) { cy += move_step; moved = 1; }
            if (keydown(KEY_0)) { cy -= move_step; moved = 1; }

            float zf = is_slow ? 1.018f : (shift_held ? 1.14f : 1.07f);
            if (keydown(KEY_ADD)) { focal = focal * zf; if (focal > 2800.0f) focal = 2800.0f; moved = 1; }
            if (keydown(KEY_SUB)) { focal = focal / zf; if (focal < 25.0f)   focal = 25.0f;   moved = 1; }

            if (moved) {
                if (alpha_held) alpha_chord_used = 1;
                dirty = 1;
            }

            if (dirty) {
                uint32_t t0_rtc = (uint32_t)rtc_ticks();

                int32_t cam_x = (int32_t)(cx * (float)FP_RAD);
                int32_t cam_y = (int32_t)(cy * (float)FP_RAD);
                int32_t cam_z = (int32_t)(cz * (float)FP_RAD);
                int32_t rx_q = (int32_t)(rx * 16384.0f), ry_q = (int32_t)(ry * 16384.0f), rz_q = (int32_t)(rz * 16384.0f);
                int32_t ux_q = (int32_t)(ux * 16384.0f), uy_q = (int32_t)(uy * 16384.0f), uz_q = (int32_t)(uz * 16384.0f);
                int32_t fx_q = (int32_t)(fx * 16384.0f), fy_q = (int32_t)(fy * 16384.0f), fz_q = (int32_t)(fz * 16384.0f);
                int32_t focal_i = (int32_t)(focal + 0.5f);

                /*
                 * Right-handed world-space headlight (+flip_sign!):
                 * L_w = normalize(+0.35*R + 0.50*U - 0.79*F) * (+flip_sign * 256)
                 * Evaluated with Valve Half-Lambert so every face angle is bright & distinct!
                 */
                float ls = (float)flip_sign * 256.0f;
                int32_t lx_w = (int32_t)((0.35f * rx + 0.50f * ux - 0.79f * fx) * ls);
                int32_t ly_w = (int32_t)((0.35f * ry + 0.50f * uy - 0.79f * fy) * ls);
                int32_t lz_w = (int32_t)((0.35f * rz + 0.50f * uz - 0.79f * fz) * ls);

                clear_viewport_and_zbuf(gray_pal[29], !wire);

                if (!model.stream_only && pverts) {
                    const int nv = model.n_verts;
                    const vfp3_t * restrict sv = model.verts;
                    pvert_t * restrict pv = pverts;

                    for (int i = 0; i < nv; i++) {
                        int32_t dx = sv[i].x - cam_x;
                        int32_t dy = sv[i].y - cam_y;
                        int32_t dz = sv[i].z - cam_z;
                        int32_t vx = (int32_t)(((int64_t)dx * rx_q + (int64_t)dy * ry_q + (int64_t)dz * rz_q) >> 14);
                        int32_t vy = (int32_t)(((int64_t)dx * ux_q + (int64_t)dy * uy_q + (int64_t)dz * uz_q) >> 14);
                        int32_t vz = (int32_t)(((int64_t)dx * fx_q + (int64_t)dy * fy_q + (int64_t)dz * fz_q) >> 14);
                        pv[i].vx = vx;
                        pv[i].vy = vy;
                        pv[i].vz = vz;

                        if (vz < NEAR_FP) {
                            pv[i].outcode = OUT_NEAR;
                            pv[i].sx = 0; pv[i].sy = 0; pv[i].sz = 65535;
                        } else {
                            int64_t inv_vz = (1LL << 36) / (int64_t)vz;
                            int64_t px = CX + ((((int64_t)vx * focal_i) * inv_vz) >> 36);
                            int64_t py = CY - ((((int64_t)vy * focal_i) * inv_vz) >> 36);
                            uint8_t oc = 0;
                            if (px < 0)        oc |= OUT_LEFT;
                            if (px >= DWIDTH)  oc |= OUT_RIGHT;
                            if (py < VIEW_TOP) oc |= OUT_TOP;
                            if (py > VIEW_BOT) oc |= OUT_BOT;
                            if (px < -GUARD_XY || px > GUARD_XY || py < -GUARD_XY || py > GUARD_XY)
                                oc |= OUT_GUARD;
                            pv[i].sx = (int16_t)(px < -32760 ? -32760 : (px > 32760 ? 32760 : px));
                            pv[i].sy = (int16_t)(py < -32760 ? -32760 : (py > 32760 ? 32760 : py));
                            int32_t iz = (int32_t)((((int64_t)NEAR_FP * 65534LL) * inv_vz) >> 36) + 1;
                            if (iz < 1) iz = 1;
                            if (iz > 65535) iz = 65535;
                            pv[i].sz = (uint16_t)iz;
                            pv[i].outcode = oc;
                        }
                    }

                    uint16_t bucket_head[16];
                    for (int b = 0; b < 16; b++) bucket_head[b] = 0xFFFF;
                    int n_vis = 0;

                    const int nt = model.n_tris;
                    const tri_t * restrict tr = model.tris;
                    const uint16_t wire_col = gray_pal[3];

                    for (int t = 0; t < nt; t++) {
                        const pvert_t *v0 = &pv[tr[t].a];
                        const pvert_t *v1 = &pv[tr[t].b];
                        const pvert_t *v2 = &pv[tr[t].c];

                        uint8_t oc_and = v0->outcode & v1->outcode & v2->outcode;
                        if (oc_and & (OUT_NEAR | OUT_LEFT | OUT_RIGHT | OUT_TOP | OUT_BOT))
                            continue;

                        uint16_t col = wire_col;
                        if (!wire) {
                            if (!tr[t].flags) continue;
                            /* dot_q8 in [-256..+256] -> Half-Lambert h_q8 in [0..256] */
                            int32_t dot_q8 = (tr[t].nx * lx_w + tr[t].ny * ly_w + tr[t].nz * lz_w) >> 7;
                            int32_t h_q8 = (dot_q8 + 256) >> 1;
                            if (h_q8 < 0)   h_q8 = 0;
                            if (h_q8 > 256) h_q8 = 256;
                            int lvl = 6 + ((h_q8 * 21) >> 8);
                            if (lvl > 27) lvl = 27;
                            col = gray_pal[lvl];
                        }

                        uint8_t oc_or = v0->outcode | v1->outcode | v2->outcode;
                        if (oc_or & (OUT_NEAR | OUT_GUARD)) {
                            vfp3_t p0 = { v0->vx, v0->vy, v0->vz };
                            vfp3_t p1 = { v1->vx, v1->vy, v1->vz };
                            vfp3_t p2 = { v2->vx, v2->vy, v2->vz };
                            render_tri_clipped_fp(p0, p1, p2, focal_i, flip_sign, col, 1, wire);
                            continue;
                        }

                        if (wire || !zbuf) {
                            draw_wire_seg(v0->sx, v0->sy, v1->sx, v1->sy, wire_col);
                            draw_wire_seg(v1->sx, v1->sy, v2->sx, v2->sy, wire_col);
                            draw_wire_seg(v2->sx, v2->sy, v0->sx, v0->sy, wire_col);
                            continue;
                        }

                        int cross2d = (int)(v1->sx - v0->sx) * (int)(v2->sy - v0->sy)
                                    - (int)(v1->sy - v0->sy) * (int)(v2->sx - v0->sx);
                        if (cross2d * flip_sign >= 0) continue;

                        if (vis_nodes) {
                            unsigned int z_max = v0->sz;
                            if (v1->sz > z_max) z_max = v1->sz;
                            if (v2->sz > z_max) z_max = v2->sz;
                            int b = (int)(z_max >> 12);
                            vis_nodes[n_vis].tri_idx = (uint16_t)t;
                            vis_nodes[n_vis].col = col;
                            vis_nodes[n_vis].next = bucket_head[b];
                            bucket_head[b] = (uint16_t)n_vis;
                            n_vis++;
                        } else {
                            draw_tri_zbuf_fp(v0->sx, v0->sy, v0->sz,
                                             v1->sx, v1->sy, v1->sz,
                                             v2->sx, v2->sy, v2->sz, col);
                        }
                    }

                    if (!wire && zbuf && vis_nodes) {
                        for (int b = 15; b >= 0; b--) {
                            uint16_t idx = bucket_head[b];
                            while (idx != 0xFFFF) {
                                const vis_node_t *nd = &vis_nodes[idx];
                                const tri_t *tp = &tr[nd->tri_idx];
                                const pvert_t *v0 = &pv[tp->a];
                                const pvert_t *v1 = &pv[tp->b];
                                const pvert_t *v2 = &pv[tp->c];
                                draw_tri_zbuf_fp(v0->sx, v0->sy, v0->sz,
                                                 v1->sx, v1->sy, v1->sz,
                                                 v2->sx, v2->sy, v2->sz, nd->col);
                                idx = nd->next;
                            }
                        }
                    }
                } else {
                    int step_v = WIN_V - OVERLAP_V;
                    int n_win = (model.n_verts + step_v - 1) / step_v;
                    vfp3_t *win = malloc(WIN_V * sizeof(vfp3_t));
                    if (win) {
                        const float sfp = model.inv_rad_fp;
                        for (int w = 0; w < n_win; w++) {
                            int own_lo = w * step_v;
                            int own_hi = own_lo + step_v;
                            if (own_hi > model.n_verts) own_hi = model.n_verts;
                            int v_lo = (own_lo > OVERLAP_V) ? (own_lo - OVERLAP_V) : 0;
                            int v_hi = own_hi;

                            restore_stock_clock();
                            int fd = os_open_ro(model.path);
                            if (fd < 0) { apply_active_clock(); break; }
                            lr_init(&g_lr, fd);
                            int vi = 0;
                            while (lr_getline(&g_lr, g_line, sizeof(g_line))) {
                                const char *s = skip_ws(g_line);
                                if (s[0] == 'v' && (s[1] == ' ' || s[1] == '\t')) {
                                    if (vi >= v_lo && vi < v_hi) {
                                        float x = 0, y = 0, z = 0;
                                        s = parse_float(s + 2, &x);
                                        s = parse_float(s, &y);
                                        s = parse_float(s, &z);
                                        int32_t dx = (int32_t)((x - model.cx) * sfp) - cam_x;
                                        int32_t dy = (int32_t)((y - model.cy) * sfp) - cam_y;
                                        int32_t dz = (int32_t)((z - model.cz) * sfp) - cam_z;
                                        win[vi - v_lo].x = (int32_t)(((int64_t)dx * rx_q + (int64_t)dy * ry_q + (int64_t)dz * rz_q) >> 14);
                                        win[vi - v_lo].y = (int32_t)(((int64_t)dx * ux_q + (int64_t)dy * uy_q + (int64_t)dz * uz_q) >> 14);
                                        win[vi - v_lo].z = (int32_t)(((int64_t)dx * fx_q + (int64_t)dy * fy_q + (int64_t)dz * fz_q) >> 14);
                                    }
                                    vi++;
                                } else if (s[0] == 'f' && (s[1] == ' ' || s[1] == '\t')) {
                                    const char *p = s + 2;
                                    int i0, prev, cur;
                                    p = parse_face_v(p, vi, &i0);
                                    if (!p) continue;
                                    p = parse_face_v(p, vi, &prev);
                                    if (!p) continue;
                                    while ((p = parse_face_v(p, vi, &cur)) != NULL) {
                                        int mx = i0 > prev ? i0 : prev;
                                        if (cur > mx) mx = cur;
                                        if (mx >= own_lo && mx < own_hi &&
                                            i0 >= v_lo && i0 < v_hi &&
                                            prev >= v_lo && prev < v_hi &&
                                            cur >= v_lo && cur < v_hi) {
                                            render_tri_clipped_fp(win[i0 - v_lo], win[prev - v_lo], win[cur - v_lo],
                                                                  focal_i, flip_sign, 0, 0, wire);
                                        }
                                        prev = cur;
                                    }
                                }
                            }
                            os_close_fd(fd);
                            apply_active_clock();
                        }
                        free(win);
                    }
                }

                uint32_t dt_ticks = (uint32_t)rtc_ticks() - t0_rtc;
                last_ms = (int)((dt_ticks * 1000u) >> 7);

                /* HUD bars */
                char hleft[68], hright[56];
                drect(0, 0, DWIDTH - 1, VIEW_TOP - 1, C_BLACK);
                const char *bn = strrchr(model.path, '/');
                bn = bn ? (bn + 1) : model.path;
                snprintf(hleft, sizeof(hleft), "%.11s [%s %s %s%s]",
                         bn,
                         wire ? "WIRE" : "CULL",
                         flip_sign < 0 ? "N-" : "N+",
                         clk_names[clk_mode],
                         is_slow ? " SLW" : (shift_held ? " FST" : ""));
                dtext(4, 2, C_WHITE, hleft);
                snprintf(hright, sizeof(hright), "%dms V:%d F:%d", last_ms, model.n_verts, model.n_tris);
                dtext_opt(DWIDTH - 4, 2, gray_pal[24], C_NONE, DTEXT_RIGHT, DTEXT_TOP, hright, -1);

                drect(0, VIEW_BOT + 1, DWIDTH - 1, DHEIGHT - 1, gray_pal[26]);
                dhline(VIEW_BOT + 1, gray_pal[18]);
                dtext(4, DHEIGHT - 12, gray_pal[4],
                      "F1:Wire F2:Flip F3:Rst F4:Open ALPHA:Slow OPTN:CPU");
                dupdate();
                dirty = 0;
            }
        }

        if (vis_nodes) free(vis_nodes);
        if (pverts)    free(pverts);
        free_model(&model);

        if (quit_to_menu) {
            if (zbuf) { free(zbuf); zbuf = NULL; }
            restore_stock_clock();
            return 0;
        }
    }
}
