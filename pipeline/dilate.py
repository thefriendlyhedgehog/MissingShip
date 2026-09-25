"""Alpha-edge dilation for RGBA16 textures.

SoH filters textures (default FILTER_THREE_POINT), so at a 1-bit-alpha edge it interpolates the
RGB of neighbouring *transparent* texels into the visible pixel. ML's foliage textures keep leftover
brown/tan artwork underneath their transparent areas, which bleeds outward as a bright halo around
every leaf. N64 hardware point-samples, so this never showed on console.

Fix: for every texel whose alpha bit is clear, replace its RGB with the average of its opaque
neighbours (repeated until the transparent region is filled). Alpha stays 0, so nothing new becomes
visible and point-sampled output is byte-equivalent in appearance -- only the interpolated edge
changes. This is the standard "alpha bleed" step in texture pipelines.
"""
import struct

def dilate_rgba16(buf, off, w, h, passes=4):
    """In-place dilation of an RGBA16 texture living at buf[off:off+w*h*2]."""
    def get(x, y):
        i = off + (y*w + x)*2
        return (buf[i] << 8) | buf[i+1]
    def put(x, y, v):
        i = off + (y*w + x)*2
        buf[i] = (v >> 8) & 0xFF
        buf[i+1] = v & 0xFF
    changed_total = 0
    for _ in range(passes):
        # snapshot opacity so a pass cannot chain off texels filled in the same pass
        opaque = [[(get(x, y) & 1) == 1 for x in range(w)] for y in range(h)]
        updates = []
        for y in range(h):
            for x in range(w):
                if opaque[y][x]:
                    continue
                rs = gs = bs = n = 0
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        nx, ny = x+dx, y+dy
                        if dx == 0 and dy == 0: continue
                        if not (0 <= nx < w and 0 <= ny < h): continue
                        if not opaque[ny][nx]: continue
                        v = get(nx, ny)
                        rs += (v >> 11) & 31; gs += (v >> 6) & 31; bs += (v >> 1) & 31; n += 1
                if n:
                    updates.append((x, y, (((rs//n) & 31) << 11) | (((gs//n) & 31) << 6)
                                          | (((bs//n) & 31) << 1)))   # alpha bit stays 0
        if not updates:
            break
        for x, y, v in updates:
            put(x, y, v)
        changed_total += len(updates)
    return changed_total
