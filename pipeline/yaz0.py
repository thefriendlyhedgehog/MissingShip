import struct

def decompress(src):
    assert src[:4] == b'Yaz0', "not Yaz0"
    size = struct.unpack('>I', src[4:8])[0]
    out = bytearray(size)
    sp = 16; dp = 0
    code = 0; codebits = 0
    while dp < size:
        if codebits == 0:
            code = src[sp]; sp += 1; codebits = 8
        if code & 0x80:
            out[dp] = src[sp]; sp += 1; dp += 1
        else:
            b1 = src[sp]; b2 = src[sp+1]; sp += 2
            dist = ((b1 & 0xF) << 8) | b2
            copy_src = dp - (dist + 1)
            n = b1 >> 4
            if n == 0:
                n = src[sp] + 0x12; sp += 1
            else:
                n += 2
            for _ in range(n):
                out[dp] = out[copy_src]; copy_src += 1; dp += 1
        code <<= 1; codebits -= 1
    return bytes(out)

def get_file(rom, vs, ve, rs, re):
    """Return decompressed contents of a dmadata entry."""
    if re == 0:  # uncompressed
        return rom[rs:rs + (ve - vs)]
    return decompress(rom[rs:re])
