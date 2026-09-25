# ML actor inventory

Every actor type placed in ML's scenes whose overlay differs from PAL MQ (Debug), checked instruction by
instruction with relocations masked (`tools/modcheck.py`). "REWRITTEN" = different section sizes; a large "N text
words differ" with equal sizes usually means a few inserted/removed instructions shifting the rest. Regenerate
with the survey in `tools/` (see PORTING_ACTORS.md). Ported ones say so in the Status column.

| Actor | Change | Placed in | Status |
|---|---|---|---|
| Ml_Fishpond | unreadable (error) | {'turibori': 13} | ported |
| Ml_TreeBridge | unreadable (error) | {'spot04': 1} | ported |
| Ml_Villager | unreadable (error) | {'spot01': 2} | ported |
| En_Firefly | 0 text / 1 data words differ | {'ydan': 2, 'HAKAdan': 7, 'kakariko': 6, 'spot00': 4, 'spot10': 11} | ported |
| En_Item00 | unreadable (error) | {'HAKAdan': 1, 'kokiri_home': 1, 'malon_stable': 3, 'spot01': 2, 'spot04': 3, 'spot09': 6, 'spot11': 10, 'spot12': 7, 'spot20': 3} | unchanged (lives in code) |
| En_Niw | 0 text / 1 data words differ | {'malon_stable': 4, 'spot20': 4} | no change (padding byte) |
| ACTOR_UNSET_1A | unreadable (error) | {'HAKAdan': 1} | todo |
| En_Reeba | REWRITTEN (text 0x130 vs 0x1890) | {'spot09': 1} | ported (Ml_BeamHeart: Blade Beam Piece of Heart) |
| En_Peehat | REWRITTEN (text 0x3290 vs 0x3280) | {'Bmori1': 1} | todo |
| ACTOR_UNSET_1F | unreadable (error) | {'link_home': 1} | ported (Ml_Prologue) |
| Boss_Dodongo | REWRITTEN (text 0xb40 vs 0x5e50) | {'Bmori1': 2} | todo |
| En_Dodojr | REWRITTEN (text 0x1ac0 vs 0x1ad0) | {'spot01': 4} | todo |
| En_St | 0 text / 1 data words differ | {'HAKAdan': 8, 'spot12': 1} | ported (not hostile) |
| En_A_Obj | unreadable (error) | {'spot12': 1} | unchanged (lives in code) |
| En_Ossan | 5818 text / 0 data words differ | {'night_shop': 1} | todo |
| Bg_Treemouth | REWRITTEN (text 0x230 vs 0x920) | {'spot00': 3, 'spot10': 6} | ported |
| Bg_Hidan_Dalm | REWRITTEN (text 0x100 vs 0x660) | {'HAKAdan': 1} | todo |
| En_Dekubaba | 1 text / 1 data words differ | {'ydan': 5, 'HAKAdan': 12, 'spot09': 5, 'spot10': 3, 'spot12': 9, 'testroom': 2} | ported |
| En_Dekunuts | 1 text / 0 data words differ  hooks ['0x806008e0'] | {'ydan': 3, 'spot10': 3} | ported |
| Bg_Toki_Hikari | REWRITTEN (text 0x80 vs 0xd50) | {'spot01': 1} | todo |
| Bg_Mori_Elevator | REWRITTEN (text 0x280 vs 0xa10) | {'HAKAdan': 1} | todo |
| En_Floormas | REWRITTEN (text 0x32e0 vs 0x2f40) | {'HAKAdan': 1} | todo |
| En_Rd | REWRITTEN (text 0x2490 vs 0x24a0) | {'night_shop': 1, 'spot01': 6} | todo |
| Bg_Po_Event | REWRITTEN (text 0x170 vs 0x1aa0) | {'night_shop': 1, 'spot01': 2, 'hairal_niwa2': 2} | todo |
| En_Sw | 0 text / 1 data words differ | {'HAKAdan': 8} | ported (not hostile) |
| En_Du | REWRITTEN (text 0x2e0 vs 0x15e0) | {'Bmori1': 3} | todo |
| Door_Ana | 3 text / 8 data words differ | {'spot09': 1, 'spot10': 3, 'spot12': 3} | ported |
| Bg_Spot02_Objects | 1131 text / 4 data words differ | {'spot01': 3} | todo |
| Boss_Va | REWRITTEN (text 0x2c00 vs 0xcc80) | {'HAKAdan_bs': 1} | todo |
| En_Nb | REWRITTEN (text 0x590 vs 0x3550) | {'Bmori1': 3} | todo |
| Bg_Spot16_Bombstone | 322 text / 0 data words differ | {'spot01': 1} | todo |
| Bg_Hidan_Kowarerukabe | 0 text / 7 data words differ | {'ydan': 1} | ported (object_ml_crackwall) |
| Boss_Tw | REWRITTEN (text 0x490 vs 0x11ae0) | {'HAKAdan': 26} | todo |
| Ml_LiftBlock | REWRITTEN (text 0x130 vs 0x540) | {'spot10': 1} | ported |
| En_Ma1 | REWRITTEN (text 0xbf0 vs 0xf90) | {'spot20': 1} | ported (Ml_Malon: new model object_ml_ma1, race host + finish line; spawns Ml_Cucco) |
| Obj_Oshihiki | 0 text / 6 data words differ | {'ydan': 2, 'HAKAdan': 1} | ported (ML colours) |
| En_Poh (not placed; spawned by Ml_Malon) | ML's En_Niw copy, object 0x09 recoloured cucco | Ml_Malon params 4 | ported (Ml_Cucco: En_Niw variant, health 0x77, no CRY_M/CRY_A) |
| En_Skj | REWRITTEN (text 0x4c0 vs 0x3390) | {'spot09': 2} | ported (Ml_SkullKid) |
| Demo_Shd | REWRITTEN (text 0x1040 vs 0x430) | {'spot11': 1} | ported (Ml_DnsShop: En_Dns copy, grotto heart-piece scrub) |
| En_Dns | REWRITTEN (text 0x1080 vs 0x1070) | {'spot11': 2, 'spot12': 3} | ported |
| En_Fz | REWRITTEN (text 0xba0 vs 0x1db0) | {'spot09': 11, 'spot20': 1, 'testroom': 1} | partial: modes 2,5,6,7 (Ml_EventSpawner) |
| Obj_Bombiwa | 0 text / 1 data words differ | {'spot01': 4, 'spot09': 1} | ported (update-culling off) |
| Ml_Lift | REWRITTEN (text 0x270 vs 0x8d0) | {'HAKAdan': 2, 'spot10': 2, 'spot12': 1} | ported |
| En_Daiku | REWRITTEN (text 0x1500 vs 0x1510) | {'spot01': 2} | todo |
| En_Niw_Lady | REWRITTEN (text 0x16d0 vs 0x1740) | {'spot10': 1} | certificate hand-in only (actor is a full rewrite) |
| En_Ms | 342 text / 0 data words differ | {'spot12': 1} | ported (Bunny Hood for Deku Nuts) |
| En_Hs | 546 text / 0 data words differ | {'spot01': 1} | todo |
| En_Wonder_Talk | REWRITTEN (text 0x5f0 vs 0x860) | {'kokiri_home4': 1, 'kakariko3': 7} | ported (text = params + 0x300, no kill, no follow-up) |
| En_Ko | 544 text / 0 data words differ  hooks ['0x80602200'] | {'spot04': 5} | not yet (shares Mido hook) |
| En_Weather_Tag | REWRITTEN (text 0xdb0 vs 0xe00) | {'spot12': 1} | ported (no story-flag kills) |
| En_Md | 263 text / 0 data words differ  hooks ['0x80602200'] | {'spot04': 1} | ported |
| En_Hy | REWRITTEN (text 0x28c0 vs 0x28f0) | {'malon_stable': 2, 'spot20': 1} | certificate offer only |
| En_Mag | REWRITTEN (text 0x1730 vs 0x23a0) | {'spot00': 1} | todo |
| Ml_FogWall | REWRITTEN (text 0xe70 vs 0x4f50) | {'spot10': 9} | ported |
| Boss_Ganon2 | REWRITTEN (text 0x1690 vs 0x9e00) | {'kakariko3': 1} | todo |
| En_Kakasi | REWRITTEN (text 0x390 vs 0xc70) | {'HAKAdan': 2} | todo |
| En_Ssh | REWRITTEN (text 0x20e0 vs 0x2150) | {'HAKAdan': 4} | todo |
| En_Weiyer | REWRITTEN (text 0x1910 vs 0x17e0) | {'ydan': 1} | ported (Ml_HoleScrub: hole-hopping scrub) |
| Bg_Jya_1flift | REWRITTEN (text 0x540 vs 0x580) | {'Bmori1': 1} | todo |
| Ml_HintNuts | REWRITTEN (text 0x7f0 vs 0x17e0) | {'ydan': 2, 'spot09': 3, 'spot10': 2, 'spot12': 1} | ported |
| Bg_Mizu_Shutter | 252 text / 0 data words differ | {'HAKAdan': 1} | todo |
| En_Cow | 1 text / 0 data words differ | {'spot01': 1, 'spot20': 1} | ported (limb 5 drawn) |
| Obj_Makekinsuta | REWRITTEN (text 0x1a0 vs 0x1a0) | {'spot01': 1} | todo |
| Ml_SongBlock | REWRITTEN (text 0x260 vs 0xb30) | {'spot04': 1} | ported |
