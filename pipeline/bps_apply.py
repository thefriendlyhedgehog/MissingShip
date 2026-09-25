import sys, zlib

def read_varint(buf, pos):
    data = 0; shift = 1
    while True:
        x = buf[pos]; pos += 1
        data += (x & 0x7f) * shift
        if x & 0x80: break
        shift <<= 7
        data += shift
    return data, pos

def apply_bps(src, patch):
    assert patch[:4] == b'BPS1', "not a BPS1 patch"
    pos = 4
    src_size, pos = read_varint(patch, pos)
    tgt_size, pos = read_varint(patch, pos)
    meta_size, pos = read_varint(patch, pos)
    pos += meta_size
    assert len(src) == src_size, f"source size mismatch: got {len(src)}, want {src_size}"

    out = bytearray(tgt_size)
    out_off = 0; src_rel = 0; tgt_rel = 0
    end = len(patch) - 12
    while pos < end:
        data, pos = read_varint(patch, pos)
        action = data & 3
        length = (data >> 2) + 1
        if action == 0:      # SourceRead
            out[out_off:out_off+length] = src[out_off:out_off+length]
            out_off += length
        elif action == 1:    # TargetRead
            out[out_off:out_off+length] = patch[pos:pos+length]
            pos += length; out_off += length
        elif action == 2:    # SourceCopy
            d, pos = read_varint(patch, pos)
            src_rel += (-1 if d & 1 else 1) * (d >> 1)
            for _ in range(length):
                out[out_off] = src[src_rel]; out_off += 1; src_rel += 1
        else:                # TargetCopy
            d, pos = read_varint(patch, pos)
            tgt_rel += (-1 if d & 1 else 1) * (d >> 1)
            for _ in range(length):
                out[out_off] = out[tgt_rel]; out_off += 1; tgt_rel += 1

    f_src, f_tgt, f_pat = (int.from_bytes(patch[end+i*4:end+i*4+4], 'little') for i in range(3))
    got_src = zlib.crc32(src) & 0xffffffff
    got_tgt = zlib.crc32(bytes(out)) & 0xffffffff
    got_pat = zlib.crc32(patch[:end+8]) & 0xffffffff
    print(f"source crc: got {got_src:08x} want {f_src:08x} {'OK' if got_src==f_src else 'MISMATCH'}")
    print(f"target crc: got {got_tgt:08x} want {f_tgt:08x} {'OK' if got_tgt==f_tgt else 'MISMATCH'}")
    print(f"patch  crc: got {got_pat:08x} want {f_pat:08x} {'OK' if got_pat==f_pat else 'MISMATCH'}")
    assert got_src == f_src and got_tgt == f_tgt, "CRC verification failed"
    return bytes(out)

src = open(sys.argv[1],'rb').read()
patch = open(sys.argv[2],'rb').read()
out = apply_bps(src, patch)
open(sys.argv[3],'wb').write(out)
print(f"wrote {sys.argv[3]} ({len(out)} bytes)")
