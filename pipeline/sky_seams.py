"""Seamless versions of ML's two skyboxes (owner's request, presentation only).

The normal sky is a cube (z_vr_box.c func_800AE2C0): four sides from Tex1-4 (128x64 CI8, row 0 at the cube's top
edge, the horizon at the bottom) and a top from Tex5 (128x128), each face spanning texels 0-124. Vanilla's Tex5 is an
overhead view whose edges continue the sides; ML's is two more horizon pictures, and ML's side picture does not wrap
left to right, so the sky showed hard seams looking up (ML's code is vanilla's, so ML looks the same on hardware).

Here the side faces' shared edges are blended into each other, and Tex5 is rebuilt from the sides: each edge of the
top face copies the top rows of the side it meets (mirrored upward), fading to the sides' average top colour at the
zenith. Everything is quantised back to ML's own palette, which is left untouched.
"""
import struct

SPAN = 124          # texels each face spans
BLEND = 16          # columns blended on each side of a side-face edge

# Cube faces (func_800AE2C0 / D_8012AF0C): face i uses Tex(i+1). Going around, face A's right edge (s = 124) meets
# face B's left edge (s = 0) for these pairs.
RIGHT_NEIGHBOUR = {0: 3, 3: 1, 1: 2, 2: 0}


def _pal(tlut):
    return [(((v >> 11) & 31) * 8, ((v >> 6) & 31) * 8, ((v >> 1) & 31) * 8)
            for v in struct.unpack('>%dH' % (len(tlut) // 2), tlut)]


def _nearest(pal, cache={}):
    key = id(pal)

    def f(c):
        c = tuple(max(0, min(255, round(x))) for x in c)
        k = (key, c)
        if k not in cache:
            cache[k] = min(range(len(pal)), key=lambda i: sum((a - b) ** 2 for a, b in zip(pal[i], c)))
        return cache[k]
    return f


def _bilinear(img, w, h, s, t):
    s = max(0.0, min(w - 1.0, s))
    t = max(0.0, min(h - 1.0, t))
    s0, t0 = int(s), int(t)
    s1, t1 = min(s0 + 1, w - 1), min(t0 + 1, h - 1)
    fs, ft = s - s0, t - t0
    out = []
    for k in range(3):
        a = img[t0][s0][k] * (1 - fs) + img[t0][s1][k] * fs
        b = img[t1][s0][k] * (1 - fs) + img[t1][s1][k] * fs
        out.append(a * (1 - ft) + b * ft)
    return out


def make_seamless(sides, top, tlut):
    """sides: four 128x64 CI8 byte strings, top: 128x128 CI8, tlut: palette bytes -> (sides, top) rewritten."""
    pal = _pal(tlut)
    q = _nearest(pal)
    img = [[[list(pal[d[t * 128 + s] % len(pal)]) for s in range(128)] for t in range(64)] for d in sides]

    # 1. Side-face edges: shift the BLEND columns either side of each shared edge so both meet the same colour.
    for a, b in RIGHT_NEIGHBOUR.items():
        A, B = img[a], img[b]
        for t in range(64):
            edge = [(A[t][SPAN][k] + B[t][0][k]) / 2 for k in range(3)]
            da = [edge[k] - A[t][SPAN][k] for k in range(3)]
            db = [edge[k] - B[t][0][k] for k in range(3)]
            for i in range(BLEND + 1):
                w = (i / BLEND) ** 2
                for k in range(3):
                    A[t][SPAN - BLEND + i][k] += w * da[k]
                    B[t][BLEND - i][k] += w * db[k]
    for a, b in RIGHT_NEIGHBOUR.items():          # texels past 124 continue into the neighbour (filtering reads them)
        for t in range(64):
            for i in range(1, 128 - SPAN):
                img[a][t][SPAN + i] = list(img[b][t][i])

    # 2. The top face (face 4): s runs with x from -64, t runs with z from +64 (see the module docstring).
    zenith = [sum(img[f][0][s][k] for f in range(4) for s in range(SPAN + 1)) / (4 * (SPAN + 1)) for k in range(3)]
    topimg = []
    for t in range(128):
        row = []
        for s in range(128):
            x = min(s, SPAN) * 128 / SPAN - 64
            z = 64 - min(t, SPAN) * 128 / SPAN
            m = max(abs(x), abs(z), 1e-6)
            if abs(z) >= abs(x):
                face, u = (0, x / m) if z < 0 else (1, -x / m)
            else:
                face, u = (2, -z / m) if x < 0 else (3, z / m)
            col = (u + 1) / 2 * SPAN
            r = m / 64                                        # 1 at the edge, 0 at the zenith
            side = _bilinear(img[face], 128, 64, col, (1 - r) * 16)  # the side's top rows, mirrored upward
            w = max(0.0, min(1.0, (r - 0.4) / 0.55))     # fade to the zenith colour towards the centre
            w = w * w * (3 - 2 * w)
            row.append([side[k] * w + zenith[k] * (1 - w) for k in range(3)])
        topimg.append(row)

    new_sides = [bytes(q(img[f][t][s]) for t in range(64) for s in range(128)) for f in range(4)]
    new_top = bytes(q(topimg[t][s]) for t in range(128) for s in range(128))
    return new_sides, new_top


def fix_skyboxes(added, skies=(('vr_fine1', 'gDaySkybox'), ('vr_cloud2', 'gSunsetOvercastSkybox'))):
    """Rewrite ML's skybox resources in `added` (path -> 0x50-byte header + raw texture) in place."""
    done = []
    for folder, name in skies:
        paths = [f'textures/{folder}_static/{name}{i}Tex' for i in range(1, 6)]
        tl = f'textures/{folder}_pal_static/{name}TLUT'
        if not all(p in added for p in paths) or tl not in added:
            continue
        sides = [added[p][0x50:] for p in paths[:4]]
        sides_new, top_new = make_seamless(sides, added[paths[4]][0x50:], added[tl][0x50:])
        for p, data in zip(paths, sides_new + [top_new]):
            added[p] = added[p][:0x50] + data
        done.append(folder)
    return done
