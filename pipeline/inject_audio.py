"""Port ML's audio using Ship of Harkinian's OWN extractor (ZAPD's ZAudio).

Deliberately NOT Shipify: that tool targets the legacy flow (patch_rom + patch_files.txt and
hand-edited Audio.xml offsets inside a Shipwright source tree, regenerating the whole .otr).
SoH 9.2.3 stores audio as individual v2 resources -- audio/sequences/*, audio/fonts/*,
audio/samples/*_META, magics OSEQ/OSFT/OSMP -- and ZAPD already knows how to produce them.
Using the first-party extractor means the format tracks main automatically and there is no
bespoke audio writer to maintain.

ML kept vanilla's audio table layout at the SAME code offsets (verified: SoundFontTable @0x138270
= 38 entries, SequenceTable @0x1386A0 = 110, SampleBankTable @0x138D90 = 7, and every table's
extents match ML's Audiobank/Audioseq/Audiotable exactly), so vanilla's Audio.xml works verbatim.
Requirements: dma 114 must be named `code` in the filelist, and dma 3/4/5 keep their
Audiobank/Audioseq/Audiotable names.

Needs the ZAudio zero-size-soundfont guard (see docs/zapd-missingship.patch): ML zeroes out unused
soundfonts while leaving their instrument counts, which crashed the stock extractor.
"""
from mspaths import ML_ROM, OOT_O2R, PIPE, ZAPD, ZAPD_CWD, zapd_config
from yaz0 import get_file
import subprocess, zipfile, shutil, os, struct, sys

CWD  = ZAPD_CWD
OUT  = PIPE + '/ml_audio.otr'
MOD  = PIPE + '/ml.otr'

if os.path.exists(OUT):
    os.remove(OUT)
r = subprocess.run([ZAPD, 'ed', '-se', 'OTR', '--otrfile', OUT, '--portVer', '9.1.1',
                    '-i', PIPE + '/xml/audio', '-b', PIPE + '/tml_zapd.z64',
                    '-fl', PIPE + '/filelists', '-o', PIPE + '/out',
                    '-osf', PIPE + '/out', '-gsf', '0',
                    '-rconf', zapd_config()],
                   cwd=CWD, capture_output=True, text=True)
if r.returncode != 0 or not os.path.exists(OUT):
    print(f"audio extraction FAILED (exit {r.returncode})"); sys.exit(1)

MEDIUM_RAM = 0
SAMPLE_MEDIUM_OFFSET = 0x41   # resource header (0x40) + codec byte; see AudioSampleFactory.cpp


def normalise_sample(name, data):
    """Force samples to MEDIUM_RAM.

    ML authored its new samples as MEDIUM_CART (streamed from ROM by DMA while playing). On N64 that
    works; in SoH every sample is already resident in RAM, but AudioSynth_ProcessNote still routes
    any non-RAM medium through AudioLoad_DmaSampleData, which treats the RAM pointer as a cartridge
    address and memmoves from garbage -- a SIGSEGV on the audio thread the first time an instrument
    using one plays (Deep Woods, font 12). Vanilla's 449 samples are all MEDIUM_RAM.
    """
    if (name.startswith('audio/samples/') and name.endswith('_META') and data[4:8] == b'PMSO'
            and data[SAMPLE_MEDIUM_OFFSET] != MEDIUM_RAM):
        return data[:SAMPLE_MEDIUM_OFFSET] + bytes([MEDIUM_RAM]) + data[SAMPLE_MEDIUM_OFFSET + 1:], True
    return data, False


SAMPLE_DATA_OFFSET = 0x48     # after codec, medium, two flag bytes and the u32 data size
MAX_BOOK_BYTES = 0x100        # SoH's mixer copies 16 * order * npredictors bytes into its int16_t[8][2][8] adpcm_table


def sample_tail(data):
    """Offset of a sample's loop + codebook (everything after the sample data), and whether that codebook is sane."""
    size, = struct.unpack_from('<I', data, SAMPLE_DATA_OFFSET - 4)
    o = SAMPLE_DATA_OFFSET + size
    nstates, = struct.unpack_from('<I', data, o + 12)
    order, npredictors, nbooks = struct.unpack_from('<iiI', data, o + 16 + 2 * nstates)
    sane = 0 < order and 0 < npredictors and 16 * order * npredictors <= MAX_BOOK_BYTES and nbooks == 8 * order * npredictors
    return o, sane


def repair_book(name, data, vanilla):
    """Give back the loop and codebook ML's ROM lost on four vanilla samples.

    ML rebuilt soundfont 0 with null loop/book pointers on the headers of Adult Link - Falling 1 (sfx 15), Child Link -
    Sneeze (sfx 69), People Chattering and Shattered Glass (instrument 79), while keeping their sample data byte for
    byte. ZAudio resolves a null pointer to the font's start and reads its pointer table as the book: order 0x180,
    npredictors 0x190, so it copies 1.2M entries, running ~2.3 MB past the end of Audiobank into whatever the heap
    holds. That made these four resources change on every build, and playing one would have SoH's mixer memcpy
    16 * order * npredictors bytes into its 256-byte ADPCM table. The data is vanilla's, so vanilla's loop and book are
    the ones that decode it; take them from oot.o2r, and fail the build for any bad book that has no vanilla twin.
    """
    if not (name.startswith('audio/samples/') and name.endswith('_META') and data[4:8] == b'PMSO'):
        return data, False
    tail, sane = sample_tail(data)
    if sane:
        return data, False
    ref = vanilla.read(name) if name in vanilla.namelist() else None
    if ref is None or ref[SAMPLE_DATA_OFFSET - 4:sample_tail(ref)[0]] != data[SAMPLE_DATA_OFFSET - 4:tail]:
        print(f"audio: {name} has an unusable codebook and no identical vanilla sample to take one from"); sys.exit(1)
    return data[:tail] + ref[sample_tail(ref)[0]:], True


def _ml_audio_files():
    """ML's code, Audiobank and Audiotable, straight from its ROM (dma 114, 3, 5)."""
    rom = open(ML_ROM, 'rb').read()
    dma, files = 0x12F70, []
    while struct.unpack_from('>IIII', rom, dma) != (0, 0, 0, 0):
        files.append(struct.unpack_from('>IIII', rom, dma)); dma += 16
    return get_file(rom, *files[114]), get_file(rom, *files[3])


def ml_font_tails():
    """{font index: {slot: sample tail as SoH writes it}} from ML's own sample headers.

    A slot is ('drum', k), ('inst', k, part) or ('sfx', k). A header with a null loop or book pointer is left out
    (repair_book covers those four samples).
    """
    code, bank = _ml_audio_files()
    fonts = {}
    for i in range(struct.unpack_from('>H', code, 0x138270)[0]):
        base, size, _, _, sd1, sd2, nsfx = struct.unpack_from('>IIBBHHH', code, 0x138280 + 16 * i)
        if size == 0:
            continue
        ninst, ndrum = sd2 >> 8, sd2 & 0xFF
        tails = fonts[i] = {}

        def header(slot, off):
            if not off:
                return
            loop, book = struct.unpack_from('>II', bank, base + off + 8)
            if not loop or not book:
                return
            start, end, count = struct.unpack_from('>III', bank, base + loop)
            states = struct.unpack_from('>16h', bank, base + loop + 16) if count else ()
            order, npred = struct.unpack_from('>ii', bank, base + book)
            entries = struct.unpack_from('>%dh' % (8 * order * npred), bank, base + book + 8)
            tails[slot] = (struct.pack('<IIII', start, end, count, len(states)) + struct.pack('<%dh' % len(states), *states)
                           + struct.pack('<iiI', order, npred, len(entries)) + struct.pack('<%dh' % len(entries), *entries))

        drums, sfx = struct.unpack_from('>II', bank, base)
        for k, inst in enumerate(struct.unpack_from('>%dI' % ninst, bank, base + 8)):
            for part in range(3) if inst else ():
                header(('inst', k, part), struct.unpack_from('>I', bank, base + inst + 8 + 8 * part)[0])
        for k, drum in enumerate(struct.unpack_from('>%dI' % ndrum, bank, base + drums) if drums else ()):
            header(('drum', k), struct.unpack_from('>I', bank, base + drum + 4)[0] if drum else 0)
        for k in range(nsfx if sfx else 0):
            header(('sfx', k), struct.unpack_from('>I', bank, base + sfx + 8 * k)[0])
    return fonts


def font_sample_refs(data):
    """(font index, [(slot, start, end, sample name)]) for a soundfont resource; start/end span the name's u32 length
    and characters. Follows ResourceFactoryBinaryAudioSoundFontV2."""
    o = 0x40
    index, = struct.unpack_from('<i', data, o); o += 4 + 2 + 6
    ndrum, ninst, nsfx = struct.unpack_from('<III', data, o); o += 12
    refs = []

    def envelope():
        nonlocal o
        n, = struct.unpack_from('<I', data, o); o += 4 + 4 * n

    def sound(slot):
        nonlocal o
        has = data[o]; o += 1
        n, = struct.unpack_from('<I', data, o)
        if has:
            refs.append((slot, o, o + 4 + n, data[o + 4:o + 4 + n].decode()))
        o += 4 + n + 4                      # name, tuning

    for k in range(ndrum):
        o += 3; envelope(); sound(('drum', k))
    for k in range(ninst):
        o += 5; envelope()
        for part in range(3):
            o += 1
            if data[o - 1]:
                sound(('inst', k, part))
    for k in range(nsfx):
        o += 1
        if data[o - 1]:
            sound(('sfx', k))
    return index, refs


def split_shared_samples(res, vanilla):
    """Give every loop/codebook a sample resource of its own.

    ZAudio makes one resource per sample-data offset, but ML's fonts can point two headers with different loops and
    codebooks at the same data. ML's soundfont 0 does this for three new sounds (instruments 81 and 83, sfx 115),
    reusing the data of Dodongo Roar, Accordion and Girl Startled 2 with loop end 7473 and a 2x2 book; whichever
    header was extracted last won, so the shops' Accordion lost its sustain loop and font 1's two sounds got a
    codebook of the wrong size. Each name keeps vanilla's loop and book where some font uses them; every other
    variant becomes '<name>_F<font>' and the fonts that use it are pointed there.
    """
    ml = ml_font_tails()
    fonts = {n: font_sample_refs(d) for n, d in res.items() if n.startswith('audio/fonts/')}
    wanted = {}                             # sample name -> {tail: [(font resource, span)]}
    for fname, (index, refs) in fonts.items():
        for slot, start, end, sample in refs:
            tail = ml.get(index, {}).get(slot)
            if tail is not None and sample in res:
                wanted.setdefault(sample, {}).setdefault(tail, []).append((fname, index, start, end))
    split = []
    renames = {}                            # font resource -> [(start, end, new name)]
    for sample, tails in wanted.items():
        cut, _ = sample_tail(res[sample])
        current = res[sample][cut:]
        if list(tails) == [current]:
            continue
        ref = vanilla.read(sample) if sample in vanilla.namelist() else None
        keep = ref[sample_tail(ref)[0]:] if ref is not None and ref[sample_tail(ref)[0]:] in tails else \
            current if current in tails else sorted(tails)[0]
        res[sample] = res[sample][:cut] + keep
        for tail, users in tails.items():
            if tail == keep:
                continue
            variant = f'{sample[:-len("_META")]}_F{users[0][1]:02d}_META'
            res[variant] = res[sample][:cut] + tail
            split.append(variant.split('/')[-1])
            for fname, _, start, end in users:
                renames.setdefault(fname, []).append((start, end, variant))
    for fname, spans in renames.items():
        data = res[fname]
        for start, end, name in sorted(spans, reverse=True):
            data = data[:start] + struct.pack('<I', len(name)) + name.encode() + data[end:]
        res[fname] = data
    return split


vanilla = zipfile.ZipFile(OOT_O2R)
src = zipfile.ZipFile(OUT)
audio = [n for n in src.namelist() if n.startswith('audio/')]
# REPLACE, don't append: every audio resource is regenerated here, so drop any already in the archive
# first -- append-only silently kept stale resources on an incremental run.
with zipfile.ZipFile(MOD) as old, zipfile.ZipFile(MOD + '.tmp', 'w', zipfile.ZIP_DEFLATED) as z:
    for info in old.infolist():
        if not info.filename.startswith('audio/'):
            z.writestr(info, old.read(info.filename))
    res = {}
    normalised = []
    repaired = []
    for n in audio:
        data, changed = normalise_sample(n, src.read(n))
        if changed:
            normalised.append(n.split('/')[-1])
        data, changed = repair_book(n, data, vanilla)
        if changed:
            repaired.append(n.split('/')[-1])
        res[n] = data
    split = split_shared_samples(res, vanilla)
    for n in sorted(res):
        z.writestr(n, res[n])
    added = len(res)
    audio = list(res)
os.replace(MOD + '.tmp', MOD)
print(f"normalised {len(normalised)} MEDIUM_CART samples to MEDIUM_RAM: {', '.join(normalised)}")
print(f"restored vanilla loop and codebook on {len(repaired)} samples: {', '.join(repaired)}")
print(f"split {len(split)} shared samples into per-font variants: {', '.join(split)}")
print(f"injected {added} audio resources "
      f"({sum(1 for n in audio if '/sequences/' in n)} sequences, "
      f"{sum(1 for n in audio if '/fonts/' in n)} fonts, "
      f"{sum(1 for n in audio if '/samples/' in n)} samples)")
