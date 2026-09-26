"""Tall-grass under-layer (owner's request, presentation only).

ML sinks the collision of its tall-grass floors (surface material 6) 20-25 units below the grass it draws, so Link
wades through it. Child Link's camera sits low enough to slip under that grass; the grass is drawn one-sided, so
from underneath the floor vanishes and the sky shows around Link's feet. This adds, to the room that draws the
grass, a copy of each grass triangle lying over such a floor with every vertex dropped onto the collision under
it, textured like the nearest ordinary floor in that room (the owner asked for the ground's grass, not the bush
texture): that floor's material and planar UV mapping, the grass triangle's own vertex shading. It is seen only
when the camera is between the two layers. Collision and ML's grass are untouched.
"""
import struct

TALL_GRASS = 6
GRASS = 8
MAX_SINK = 60      # a drawn floor more than this above the collision is something else (a ledge, a bridge)
MIN_SINK = 6


def _cmds(d, off=0):
    while off + 8 <= len(d):
        yield off, struct.unpack('>II', d[off:off + 8])
        off += 8


def _hdr(d):
    for off, (w0, w1) in _cmds(d):
        if d[off] == 0x14:
            return
        yield d[off], d[off + 1], w1


def _collision(scene):
    col = [v for c, a, v in _hdr(scene) if c == 0x03][0] & 0xFFFFFF
    nv, = struct.unpack('>H', scene[col + 12:col + 14])
    va = struct.unpack('>I', scene[col + 16:col + 20])[0] & 0xFFFFFF
    npoly, = struct.unpack('>H', scene[col + 20:col + 22])
    pa = struct.unpack('>I', scene[col + 24:col + 28])[0] & 0xFFFFFF
    sa = struct.unpack('>I', scene[col + 28:col + 32])[0] & 0xFFFFFF
    V = [struct.unpack('>hhh', scene[va + 6 * i:va + 6 * i + 6]) for i in range(nv)]
    floors = []
    for i in range(npoly):
        ty, a, b, c, nx, ny, nz, dist = struct.unpack('>HHHHhhhh', scene[pa + 16 * i:pa + 16 * i + 16])
        if ny / 0x7FFF < 0.7:
            continue
        material = struct.unpack('>I', scene[sa + 8 * ty + 4:sa + 8 * ty + 8])[0] & 0xF
        floors.append(([V[a & 0x1FFF], V[b & 0x1FFF], V[c & 0x1FFF]], material))
    return floors


def _bary(px, pz, t, tol=0.001):
    (x1, z1), (x2, z2), (x3, z3) = [(q[0], q[2]) for q in t]
    den = (z2 - z3) * (x1 - x3) + (x3 - x2) * (z1 - z3)
    if den == 0:
        return None
    a = ((z2 - z3) * (px - x3) + (x3 - x2) * (pz - z3)) / den
    b = ((z3 - z1) * (px - x3) + (x1 - x3) * (pz - z3)) / den
    c = 1 - a - b
    if min(a, b, c) < -tol:
        return None
    return a, b, c


def _floor_below(floors, x, y, z, tol=0.001):
    """(height, material) of the highest collision floor at (x, z) no higher than y + MIN_SINK, or None."""
    best = None
    for pts, material in floors:
        w = _bary(x, z, pts, tol)
        if w:
            fy = sum(w[k] * pts[k][1] for k in range(3))
            if y - MAX_SINK <= fy <= y + MIN_SINK and (best is None or fy > best[0]):
                best = (fy, material)
    return best


def _opa_tris(room, scene):
    """Triangles of the room's opaque mesh: (raw vertex records, material block bytes)."""
    out = []

    def walk(off, depth=0):
        vb = [None] * 64
        state = []          # the current run of state commands (texture, combiner, render mode...)
        in_state = False
        while off + 8 <= len(room):
            w0, w1 = struct.unpack('>II', room[off:off + 8])
            op = w0 >> 24
            raw = room[off:off + 8]
            off += 8
            if op == 0x01:
                n = (w0 >> 12) & 0xFF
                v0 = ((w0 >> 1) & 0x7F) - n
                a = w1 & 0xFFFFFF
                src = room if w1 >> 24 == 3 else scene
                for k in range(n):
                    vb[v0 + k] = bytes(src[a + 16 * k:a + 16 * k + 16])
                in_state = False
            elif op in (0x05, 0x06):
                for ww in ((w0,) if op == 0x05 else (w0, w1)):
                    i = [((ww >> s) & 0xFF) // 2 for s in (16, 8, 0)]
                    if all(vb[j] for j in i):
                        out.append(([vb[j] for j in i], b''.join(state)))
                in_state = False
            elif op == 0xDE:
                if w1 >> 24 == 3 and depth < 8:
                    walk(w1 & 0xFFFFFF, depth + 1)
                if (w0 >> 16) & 0xFF:
                    return
            elif op == 0xDF:
                return
            elif op == 0x03:     # G_CULLDL: geometry, not state
                pass
            else:
                if not in_state:
                    state = []
                    in_state = True
                state.append(raw)

    for c, a, v in _hdr(room):
        if c == 0x0A:
            m = v & 0xFFFFFF
            t, n = room[m], room[m + 1]
            st = struct.unpack('>I', room[m + 4:m + 8])[0] & 0xFFFFFF
            sz = 8 if t == 0 else 16
            for k in range(n):
                opa = struct.unpack('>I', room[st + sz * k + sz - 8:st + sz * k + sz - 4])[0]
                if opa:
                    walk(opa & 0xFFFFFF)
    return out


def _xz_area(pos):
    return ((pos[1][0] - pos[0][0]) * (pos[2][2] - pos[0][2]) - (pos[2][0] - pos[0][0]) * (pos[1][2] - pos[0][2])) / 2


def _is_floor(pos):
    """Upward-facing and no steeper than a walkable floor (normal y >= 0.7, as collision floors)."""
    ax, ay, az = [pos[1][k] - pos[0][k] for k in range(3)]
    bx, by, bz = [pos[2][k] - pos[0][k] for k in range(3)]
    nx, ny, nz = ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx
    length = (nx * nx + ny * ny + nz * nz) ** 0.5
    return length > 0 and _xz_area(pos) < -64 and abs(ny) / length >= 0.7


def _uv_map(verts, pos):
    """Affine (x, z) -> (u, v) of an upward-facing triangle."""
    (x1, _, z1), (x2, _, z2), (x3, _, z3) = pos
    uv = [struct.unpack('>hh', v[8:12]) for v in verts]
    det = (x2 - x1) * (z3 - z1) - (x3 - x1) * (z2 - z1)

    def f(x, z):
        a = ((x - x1) * (z3 - z1) - (x3 - x1) * (z - z1)) / det
        b = ((x2 - x1) * (z - z1) - (x - x1) * (z2 - z1)) / det
        return tuple(uv[0][k] + a * (uv[1][k] - uv[0][k]) + b * (uv[2][k] - uv[0][k]) for k in range(2))
    return f


def _tile_period(material):
    """Twice the render tile's size in S10.5 units (a shift by it samples the same texels, wrapped or mirrored)."""
    for i in range(0, len(material), 8):
        w0, w1 = struct.unpack('>II', material[i:i + 8])
        if w0 >> 24 == 0xF2 and (w1 >> 24) & 7 == 0:
            return (((w1 >> 12) & 0xFFF) // 4 + 1) * 64, ((w1 & 0xFFF) // 4 + 1) * 64
    return 64 * 64, 64 * 64


def add_grass_underlayer(room, scene):
    """Append the under-layer DL to the room; return (room, [DL offset] or [], triangle count)."""
    floors = _collision(scene)
    sunk, ground = [], []
    for verts, material in _opa_tris(room, scene):
        pos = [struct.unpack('>hhh', v[:6]) for v in verts]
        if not _is_floor(pos):          # walls and slopes (ML's floors wind so their xz area is negative)
            continue
        cx = sum(p[0] for p in pos) / 3
        cy = sum(p[1] for p in pos) / 3
        cz = sum(p[2] for p in pos) / 3
        under = _floor_below(floors, cx, cy, cz)
        if under is None:
            continue
        if under[1] == TALL_GRASS and cy - under[0] >= MIN_SINK:
            sunk.append((verts, pos))
        elif under[1] != TALL_GRASS:
            ground.append(((cx, cy, cz), material, _uv_map(verts, pos), under[1]))
    if not sunk or not ground:
        return room, [], 0
    # the grass floors (material 8) when the room has any, so a nearby bridge or path is not picked
    if any(g[3] == GRASS for g in ground):
        ground = [g for g in ground if g[3] == GRASS]
    groups = {}
    for verts, pos in sunk:
        cx = sum(p[0] for p in pos) / 3
        cy = sum(p[1] for p in pos) / 3
        cz = sum(p[2] for p in pos) / 3
        _, material, uv, _ = min(ground, key=lambda g: (g[0][0] - cx) ** 2 + (g[0][2] - cz) ** 2 + 4 * (g[0][1] - cy) ** 2)
        pu, pv = _tile_period(material)
        uvs = [uv(x, z) for x, _, z in pos]
        su = pu * (min(u for u, _ in uvs) // pu)
        sv = pv * (min(v for _, v in uvs) // pv)
        drops = [_floor_below(floors, x, y, z, tol=0.02) for x, y, z in pos]
        if None in drops:               # a corner with no floor under it: leave the triangle alone
            continue
        new = []
        for v, (x, y, z), (u, t), f in zip(verts, pos, uvs, drops):
            ny = min(y, round(f[0]))    # never above ML's own surface
            u = max(-32768, min(32767, round(u - su)))
            t = max(-32768, min(32767, round(t - sv)))
            new.append(struct.pack('>hhhHhh', x, ny, z, 0, u, t) + v[12:])
        groups.setdefault(material, []).append(new)
    d = bytearray(room)
    while len(d) % 8:
        d.append(0)
    vtx = {}
    for material, tris in groups.items():
        vtx[material] = len(d)
        for t in tris:
            for v in t:
                d += v
    dl = len(d)
    count = 0
    for material, tris in groups.items():
        d += material
        for first in range(0, len(tris), 10):
            batch = tris[first:first + 10]
            n = 3 * len(batch)
            d += struct.pack('>II', 0x01000000 | (n << 12) | (n * 2), 0x03000000 | (vtx[material] + 48 * first))
            for k in range(0, len(batch), 2):
                i = 3 * k
                if k + 1 < len(batch):
                    d += struct.pack('>II', 0x06000000 | (i * 2 << 16) | ((i + 1) * 2 << 8) | ((i + 2) * 2),
                                     ((i + 3) * 2 << 16) | ((i + 4) * 2 << 8) | ((i + 5) * 2))
                else:
                    d += struct.pack('>II', 0x05000000 | (i * 2 << 16) | ((i + 1) * 2 << 8) | ((i + 2) * 2), 0)
            count += len(batch)
    d += struct.pack('>II', 0xDF000000, 0)
    return bytes(d), [dl], count
