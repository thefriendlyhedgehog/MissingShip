from mspaths import PIPE
import json
m=json.load(open(PIPE + '/inlined.json'))
try: rl=json.load(open(PIPE + '/roomlocal.json'))
except Exception: rl={}
scene_tex=json.load(open(PIPE + '/scene_textures.json'))
FMT={0:'rgba',2:'ci',3:'ia',4:'i'}; SIZ={0:'4',1:'8',2:'16',3:'32'}
import sys as _s, os as _o; _s.path.insert(0,PIPE)
from ml_map import SCENES
# every ML scene goes in one flat folder; ZAPD exports OoT scenes to scenes/shared/<name>_scene
# regardless of input folder (the nonmq/mq branch is MM-gated), so the folder name is cosmetic.
# DisplayListExporter.cpp picks scenes/nonmq (instead of scenes/shared) only when the XML path
# contains 'dungeons/' AND the filename matches its MQ-variant dungeon regex. OTRPlay_SpawnScene
# looks those scenes up under nonmq/, so they MUST be exported there or the scene loads as nullptr.
MQ_DUNGEONS={'ydan','ddan','bdan','Bmori1','HIDAN','MIZUsin','jyasinzou','HAKAdan','HAKAdanCH',
             'ice_doukutu','men','ganontika'}
MAP={v['scene'][:-6]:('dungeons' if v['scene'][:-6] in MQ_DUNGEONS else 'ml', len(v['rooms']))
     for v in SCENES.values()}
_o.makedirs(PIPE + '/xml/scenes/dungeons', exist_ok=True)
_o.makedirs(PIPE + '/xml/scenes/ml', exist_ok=True)
VALID={'rgba16','rgba32','ci4','ci8','ia4','ia8','ia16','i4','i8'}
# ML's entrance intro cutscenes: single source of truth lives in ml_entrance.py.
from ml_entrance import ENTRANCE_CS, ACTOR_CS
for nm,(folder,nrooms) in MAP.items():
    sdecl={}
    for a_s,(fmt,w,h,tl) in sorted(scene_tex.get(nm,{}).items(), key=lambda kv:int(kv[0],16)):
        if fmt not in VALID: continue
        a=int(a_s,16)
        sdecl[a]='        <Texture Name="%s_sceneTex_%06X" OutName="%s_sceneTex_%06X" Format="%s" Width="%d" Height="%d" Offset="0x%X" AddedByScript="true"/>'%(nm,a,nm,a,fmt,w,h,a)
    for off,csname in ENTRANCE_CS.get(nm,[])+ACTOR_CS.get(nm,[]):
        sdecl[off]='        <Cutscene Name="%s" Offset="0x%X"/>'%(csname,off)
    sbody="\n".join(sdecl[k] for k in sorted(sdecl))
    rooms=[]
    for k in range(nrooms):
        key='%s_room_%d'%(nm,k); ents=m.get(key,[]); last=None; rd={}
        for e in ents:
            if e['kind']=='tlut':
                last=e['off']
                rd[e['off']]='        <Texture Name="%s_tlut_%06X" OutName="%s_tlut_%06X" Format="rgba16" Width="%d" Height="1" Offset="0x%X" AddedByScript="true"/>'%(key,e['off'],key,e['off'],e.get('count') or 16,e['off'])
                continue
            f=FMT.get(e['fmt']); s=SIZ.get(e['siz'])
            if not f: continue
            fs=f+s
            if fs not in VALID: continue
            t=' TlutOffset="0x%X"'%last if (fs.startswith('ci') and last is not None) else ''
            rd[e['off']]='        <Texture Name="%s_tex_%06X" OutName="%s_tex_%06X" Format="%s" Width="%d" Height="%d" Offset="0x%X"%s AddedByScript="true"/>'%(key,e['off'],key,e['off'],fs,e['w'],e['h'],e['off'],t)
        # Room-local ML textures ZAPD failed to auto-discover still need explicit declarations,
        # or the exporter emits its 0x08000001 fallback for them (texDecl == nullptr).
        for e in rl.get(key,[]):
            f=FMT.get(e['fmt']); sz=SIZ.get(e['siz'])
            if not f or e['addr'] in rd: continue
            fs=f+sz
            if fs not in VALID: continue
            rd[e['addr']]='        <Texture Name="%s_local_%06X" OutName="%s_local_%06X" Format="%s" Width="%d" Height="%d" Offset="0x%X" AddedByScript="true"/>'%(key,e['addr'],key,e['addr'],fs,e['w'],e['h'],e['addr'])
        rbody="\n".join(rd[x] for x in sorted(rd))
        rooms.append('    <File Name="%s" Segment="3">\n%s\n        <Room Name="%s" Offset="0x0"/>\n    </File>'%(key,rbody,key))
    open(PIPE + '/xml/scenes/%s/%s.xml'%(folder,nm),'w').write(
        '<Root>\n    <File Name="%s_scene" Segment="2">\n%s\n        <Scene Name="%s_scene" Offset="0x0"/>\n    </File>\n'%(nm,sbody,nm)+"\n".join(rooms)+'\n</Root>\n')
