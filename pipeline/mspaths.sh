# Shell side of mspaths.py (MS-001): source this, never execute it. Same variables and defaults; anything
# already set in the environment wins. Exported so the Python steps a script runs see the same values.
MS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MS_PIPE="$MS_ROOT/pipeline"
: "${MS_ML_ROM:=$MS_PIPE/tml_2_0.z64}"
: "${MS_DEBUG_ROM:=$MS_ROOT/roms/oot-mq-debug-pal.z64}"
: "${MS_OOT_O2R:=$MS_ROOT/home/oot.o2r}"
: "${MS_SHIPWRIGHT:=$MS_ROOT/engine}"
: "${MS_ZAPD:=$MS_SHIPWRIGHT/build-ml/ZAPD/ZAPD.out}"
: "${MS_ZAPD_CWD:=$MS_SHIPWRIGHT/soh}"
export MS_ROOT MS_PIPE MS_ML_ROM MS_DEBUG_ROM MS_OOT_O2R MS_SHIPWRIGHT MS_ZAPD MS_ZAPD_CWD
