"""ML entrance-table port, done entirely in scene data.

ML rebuilt gEntranceTable (code 0x10b360, 1556 x 4 bytes: u8 scene, u8 spawn, u16 flags). SoH uses
VANILLA's table, compiled in, so an ML exit-list entry resolves to the wrong destination -- this is
why loading zones went to the wrong places.

Fix without touching the engine: a scene's header command 0x13 is its EXIT LIST, a plain array of
entrance ids. For each id e, look up ML's intended destination (scene, spawn), then find a VANILLA
entrance v with the SAME (scene, spawn) and write v in its place. SoH then resolves v through its
own table and lands where ML meant. This works because ML scenes now occupy their real vanilla
scene IDs (see ml_map.py), so the scene half of the destination already agrees.
"""
from mspaths import PIPE, SOH
import re, json, struct, os

# Read the tables of the engine MissingShip actually ships (Shipwright), not SOHCombo's. Shipwright's
# entrance table carries MissingShip's 32 appended entrances for ML (scene, spawn) pairs vanilla has no
# entrance for; reading SOHCombo's unpatched copy hid them, so every exit needing one fell back to
# spawn 0 -- the "clipped camera / void on arrival" transitions.
_SHIP = SOH
_SC = open(_SHIP + '/include/tables/scene_table.h').read()
_EN = open(_SHIP + '/include/tables/entrance_table.h').read()
_enum2id = {m.group(2): int(m.group(1), 16)
            for m in re.finditer(r'/\* 0x([0-9A-Fa-f]{2}) \*/ DEFINE_SCENE\(\w+,\s*\w+,\s*(\w+),', _SC)}
VANILLA = {}
for _m in re.finditer(r'/\* 0x([0-9A-Fa-f]{3}) \*/ DEFINE_ENTRANCE\(\w+,\s*(\w+),\s*(\d+),', _EN):
    VANILLA[int(_m.group(1), 16)] = (_enum2id.get(_m.group(2)), int(_m.group(3)))
# (scene, spawn) -> lowest vanilla entrance index. Lowest matters: the entrance system expects the
# FIRST entry of a 4-entry scene-layer group and applies the layer offset itself.
REV = {}
for _i, _d in sorted(VANILLA.items()):
    if _d[0] is not None:
        REV.setdefault(_d, _i)

ML = [tuple(x) for x in json.load(open(PIPE + '/ml_entrancetable.json'))]

# ml_map moves malon_stable from its real slot 54 to slot 52 so the new game spawns somewhere real.
# ML exit targets still say scene 54, so translate them or they fall through to whatever vanilla
# puts at that id. Leaving ML entrance 108 (= scene 54 spawn 1) raw sent the player from Lon Lon
# Ranch straight to the CHAMBER OF THE SAGES, because vanilla entrance 108 is (68, 0).
# Only needed while ml_map's start-fix moves malon_stable to slot 52. With ML's real layout
# restored (ML_NO_START_FIX=1) scene 54 ships normally and must NOT be translated.
# Mirrors ml_map's start-fix, which is now opt-in: translate only when that remap is active.
SCENE_TRANSLATE = {54: 52} if os.environ.get('ML_START_FIX') else {}

def remap_exits(d, populated):
    """Rewrite scene header cmd 0x13 exit-list entries from ML ids to vanilla equivalents."""
    d = bytearray(d); off = 0; changed = 0; kept = 0
    # Exit lists have no length. Since entrance 0 is valid, the scan must stop at the next header data block
    # instead: in tokinoma the list is followed directly by the player entries, whose actor id 0 read as
    # "entrance 0" and got rewritten, so Player never spawned there.
    starts = set()
    while off + 8 <= len(d) and d[off] != 0x14:
        if d[off+4] == 0x02:
            starts.add(struct.unpack('>I', d[off+4:off+8])[0] & 0xFFFFFF)
        off += 8
    off = 0
    while off + 8 <= len(d):
        c = d[off]
        if c == 0x14:
            break
        if c == 0x13:
            ptr = struct.unpack('>I', d[off+4:off+8])[0] & 0xFFFFFF
            end = min([x for x in starts if x > ptr], default=len(d))
            k = 0
            while ptr + k*2 + 2 <= end and k < 32:
                e = struct.unpack('>H', d[ptr+k*2:ptr+k*2+2])[0]
                # ML entrance 0 is a real destination (Deep Woods, spawn 0), and ML pads unused exit slots with
                # it mid-list, so it must not end the scan: stopping here left every later exit unmapped
                # (Deku Road's last three exits, the Deku Road grotto arena's, ...).
                if e >= len(ML):
                    break
                scene, spawn, _ = ML[e]
                # Translate BEFORE the validity check: ml_map moves malon_stable off slot 54, so a
                # raw scene-54 target would look "not shipped", break the scan, and leave the exit
                # pointing at whatever vanilla has there (the Chamber of the Sages bug).
                scene = SCENE_TRANSLATE.get(scene, scene)
                # only trust an entry that targets a scene ML actually ships; anything else is
                # past the end of the list or an entrance ML never repointed
                if scene not in populated:
                    break
                v = REV.get((scene, spawn))
                if v is None:
                    # No vanilla entrance for this exact spawn. Landing at a different spawn in the
                    # RIGHT scene is far better than falling through to an unrelated scene, so take
                    # the lowest-numbered entrance that reaches the same scene.
                    same = [i for (sc, sp), i in REV.items() if sc == scene]
                    v = min(same) if same else None
                if v is not None and v != e:
                    struct.pack_into('>H', d, ptr + k*2, v); changed += 1
                else:
                    kept += 1
                k += 1
        off += 8
    return bytes(d), changed, kept


# ML's own entrance-cutscene table (code 0x101394) references scripts that no scene header points at,
# so they need explicit declarations (gen_xml_inline.py) AND destination rewriting here.
# Offsets verified as real cutscenes (command count + end frame). Skipped: ML entrance 0x420 (points at
# an all-zero offset, a stale vanilla pointer) and 0x503 (its scene is not part of the port).
ENTRANCE_CS = {'spot10': [(0x260, 'gMlSpot10IntroCs')], 'spot05': [(0x0E0, 'gMlSpot05IntroCs')],
               'spot20': [(0x3D8, 'gMlSpot20IntroCs')], 'spot12': [(0x3A0, 'gMlSpot12IntroCs')],
               'link_home': [(0x0D8, 'gMlLinkHomeIntroCs')], 'spot09': [(0x428, 'gMlSpot09IntroCs')],
               'turibori': [(0xFB90, 'gMlTuriboriIntroCs')], 'spot04': [(0x210, 'gMlSpot04IntroCs')],
               'tokinoma': [(0x1A0, 'gMlTokinomaIntroCs')]}
# Cutscenes an actor starts itself with Cutscene_SetSegment, exported under a name its C code can use.
ACTOR_CS = {'HAKAdan_bs': [(0x168, 'gMlHakadanBsIntroCs'),       # Ml_BossVa: the boss wakes (also the header's)
                           (0xACF0, 'gMlHakadanBsDeathCs'),      # Ml_BossVa: phase 1 health 0
                           (0xB050, 'gMlHakadanBsKnockdownCs')], # Ml_BossVa: phase 0 health 0
            'Bmori1': [(0x350, 'gMlBmori1ThievesLeaveCs')]}                # Ml_Thief: switch 7, the thieves run off


def _destination_codes():
    """Map CS_DESTINATION codes to entrance ids, from the engine's own destination switch.

    A terminator's `base` is a destination CODE, not an entrance: Cutscene_Command_Terminator's
    `switch (cmd->base)` (the second one in the function -- the first is an 11-case delay helper for
    55..78) turns it into an entrance. ML's copy of that switch matches vanilla's except
    for the cases in ML_PATCHED_CODES (same jump table, e.g. case 119 at 0x80066F44 in both), so SoH's
    source describes what ML does. The resulting entrance id is in ML's numbering, i.e. an index
    into ML's rebuilt entrance table -- resolved by the caller.
    """
    src = open(_SHIP + '/src/code/z_demo.c').read()
    fn = src.index('void Cutscene_Command_Terminator')
    body = src[fn:src.index('\nvoid ', fn + 10)]
    seg = body[[m.start() for m in re.finditer(r'switch \(cmd->base\)', body)][1]:]
    seg = seg[:seg.index('default:')]
    ids = {m.group(2): int(m.group(1), 16)
           for m in re.finditer(r'/\* 0x([0-9A-Fa-f]+) \*/ DEFINE_ENTRANCE\((\w+),', _EN)}
    codes = {}
    parts = re.split(r'((?:\s*case \d+:)+)', seg)
    for i in range(1, len(parts) - 1, 2):
        block = parts[i + 1].split('break;')[0]
        # SoH wraps some targets in randomizer overrides: Entrance_OverrideNextIndex(ENTR_X) means ENTR_X.
        names = re.findall(r'nextEntranceIndex = (?:Entrance_OverrideNextIndex\()?(ENTR_\w+)', block)
        for label in re.findall(r'case (\d+):', parts[i]):
            if names:
                codes[int(label)] = ids[names[0]]
    return codes


CODE_TO_ENTRANCE = _destination_codes()
# Destination cases ML itself patched; the engine's switch reproduces them, so their codes are left alone.
# 56: Kakariko gate (0xFFF4) -> ML entrance 0x420 with cutscene 0xFFF0 (code 0x80066050); only the
# Fishing Pond intro uses it.
ML_PATCHED_CODES = {56}


def _cutscene_terminators(d, cs):
    """Yield the file offset of `base` for every terminator in the cutscene at `cs`."""
    CAM = {0x0001, 0x0002, 0x0005, 0x0006, 0x0007, 0x0008}
    E12 = {0x0009, 0x008C}
    tot = struct.unpack('>I', d[cs:cs+4])[0]
    a = cs + 8
    for _ in range(tot):
        if a + 4 > len(d):
            return
        ct = struct.unpack('>I', d[a:a+4])[0]; a += 4
        if ct == 0xFFFFFFFF:
            return
        if ct in CAM:
            a += 8
            while a + 16 <= len(d):
                cont = struct.unpack('>b', d[a:a+1])[0]; a += 16
                if cont == -1:
                    break
        elif ct == 0x03E8:
            yield a + 4
            a += 12
        elif ct == 0x0013:
            n = struct.unpack('>I', d[a:a+4])[0]; a += 4 + n*12
        elif ct == 0x002D:
            a += 12
        else:
            n = struct.unpack('>I', d[a:a+4])[0]
            a += 4 + n*(12 if ct in E12 else 48)


def fix_cutscene_destinations(d, populated, name=None):
    """Rewrite CS_DESTINATION targets so SoH lands where ML does.

    code -> entrance (the engine's destination switch, identical in ML) -> ML's entrance table
    -> (scene, spawn) -> the equivalent entrance in the table MissingShip ships. The result is stored
    with bit 15 set; the engine patch's `default:` case treats that as a literal entrance index, so it
    can never collide with a destination code. Covers header cutscenes and ML's entrance intros.

    Known loss: a destination case can also set state (e.g. case 119 sets noon and a white fade);
    the bit-15 path only transitions. Previously this read `base` as an ML ENTRANCE, which sent the
    opening prologue (code 119 -> ML 0x5F0 -> tokinoma) to ML entrance 119 = the Lon Lon stable.
    """
    d = bytearray(d); fixed = 0; unresolved = []
    offs = set(o for o, _ in ENTRANCE_CS.get(name, []) + ACTOR_CS.get(name, []))
    off = 0
    while off + 8 <= len(d):
        if d[off] == 0x14:
            break
        if d[off] == 0x17:
            offs.add(struct.unpack('>I', d[off+4:off+8])[0] & 0xFFFFFF)
        off += 8
    for cs in sorted(o for o in offs if o and o + 8 <= len(d)):
        for at in _cutscene_terminators(d, cs):
            code = struct.unpack('>H', d[at:at+2])[0]
            if code & 0x8000 or code in ML_PATCHED_CODES:
                continue                                   # already rewritten, or ML's own case
            e = CODE_TO_ENTRANCE.get(code)
            if e is None or e >= len(ML):
                unresolved.append((cs, code, 'no destination case')); continue
            scene, spawn, _ = ML[e]
            scene = SCENE_TRANSLATE.get(scene, scene)
            v = REV.get((scene, spawn))
            if v is None and scene in populated:
                same = [i for (sc, sp), i in REV.items() if sc == scene]
                v = min(same) if same else None
                unresolved.append((cs, code, f'no entrance for (scene {scene:#x}, spawn {spawn}); '
                                             f'fell back to {v:#x}' if v is not None else 'none'))
            if v is None:
                unresolved.append((cs, code, f'scene {scene:#x} not shipped')); continue
            struct.pack_into('>H', d, at, 0x8000 | v)
            fixed += 1
    for cs, code, why in unresolved:
        print(f"    cs_dest {name}@{cs:#x} code {code}: {why}")
    return bytes(d), fixed
