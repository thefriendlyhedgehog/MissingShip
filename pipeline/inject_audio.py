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
import subprocess, zipfile, shutil, os, sys

ZAPD = '/Volumes/MacMiniExternal/AI_Projects/Shipwright/build-ml/ZAPD/ZAPD.out'
CWD  = '/Volumes/MacMiniExternal/AI_Projects/Shipwright/soh'
OUT  = '/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/ml_audio.otr'
MOD  = '/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/ml.otr'

if os.path.exists(OUT):
    os.remove(OUT)
r = subprocess.run([ZAPD, 'ed', '-se', 'OTR', '--otrfile', OUT, '--portVer', '9.1.1',
                    '-i', '/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/xml/audio', '-b', '/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/tml_zapd.z64',
                    '-fl', '/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/filelists', '-o', '/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/out',
                    '-osf', '/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/out', '-gsf', '0',
                    '-rconf', '/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/Config_ML.xml'],
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


src = zipfile.ZipFile(OUT)
audio = [n for n in src.namelist() if n.startswith('audio/')]
# REPLACE, don't append: every audio resource is regenerated here, so drop any already in the archive
# first -- append-only silently kept stale resources on an incremental run.
with zipfile.ZipFile(MOD) as old, zipfile.ZipFile(MOD + '.tmp', 'w', zipfile.ZIP_DEFLATED) as z:
    for info in old.infolist():
        if not info.filename.startswith('audio/'):
            z.writestr(info, old.read(info.filename))
    added = 0
    normalised = []
    for n in audio:
        data, changed = normalise_sample(n, src.read(n))
        if changed:
            normalised.append(n.split('/')[-1])
        z.writestr(n, data); added += 1
os.replace(MOD + '.tmp', MOD)
print(f"normalised {len(normalised)} MEDIUM_CART samples to MEDIUM_RAM: {', '.join(normalised)}")
print(f"injected {added} audio resources "
      f"({sum(1 for n in audio if '/sequences/' in n)} sequences, "
      f"{sum(1 for n in audio if '/fonts/' in n)} fonts, "
      f"{sum(1 for n in audio if '/samples/' in n)} samples)")
