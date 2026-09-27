"""What ML changed against the ROM it was built from, recorded so a player's build needs no debug ROM (MS-154).

Several steps ship a texture, a credit or a collision only if ML's bytes differ from the OoT PAL MQ Debug ROM's (the
baseline ML was built from), since comparing with stock alone would ship debug-vs-retail differences ML never made.
Players don't have the debug ROM, so each of those answers goes through value():

- **Record** (the debug ROM is present, the development setup): compute the answer from both ROMs, return it, and
  save it in pipeline/baseline.json, which is committed. Sanity checks against the debug ROM run too (RECORD).
- **Replay** (no debug ROM, or MS_BASELINE=replay): return the recorded answer; the ROM is never read.

baseline.json holds only true/false answers and resource names, never ROM data. Keys are per step, so a step records
its whole set at once (begin / save). A replayed build of the same inputs is byte-identical to a recorded one.
"""
import json, os, struct, sys

from mspaths import DEBUG_ROM, PIPE, SOH

FILE = os.path.join(PIPE, 'baseline.json')
RECORD = os.path.exists(DEBUG_ROM) and os.environ.get('MS_BASELINE') != 'replay'

_step, _answers, _rom, _names = None, {}, None, None


def begin(step):
    global _step
    _step = step
    _answers.clear()


def value(key, compute):
    """compute() in record mode (and remember it); the recorded answer in replay mode."""
    if RECORD:
        v = compute()
        _answers[key] = v
        return v
    try:
        return _load()[_step][key]
    except KeyError:
        sys.exit(f'{os.path.basename(FILE)} has no answer for {_step}: {key}. '
                 f'Rebuild once with the debug ROM ({DEBUG_ROM}) to record it.')


def save():
    """Write this step's answers (record mode only), keeping the other steps'."""
    if not RECORD:
        return
    data = _load() if os.path.exists(FILE) else {}
    data[_step] = dict(sorted(_answers.items()))
    with open(FILE, 'w', encoding='utf-8') as f:
        json.dump(dict(sorted(data.items())), f, indent=1)
        f.write('\n')


def debug_file(name):
    """A file of the debug ROM by its filelist name (record mode only)."""
    global _rom, _names
    assert RECORD, 'the debug ROM is only read in record mode'
    from yaz0 import get_file
    if _rom is None:
        _rom = open(DEBUG_ROM, 'rb').read()
        _names = open(os.path.join(SOH, 'assets', 'extractor', 'filelists', 'dbg.txt')).read().split('\n')
    i = _names.index(name)
    return get_file(_rom, *struct.unpack('>IIII', _rom[0x12F70 + 16 * i:0x12F70 + 16 * i + 16]))


_cache = None


def _load():
    global _cache
    if _cache is None:
        with open(FILE, encoding='utf-8') as f:
            _cache = json.load(f)
    return _cache
