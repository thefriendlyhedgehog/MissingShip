// ML's collision for its tree bridge, from ML dma 105 (actor 0x005) overlay data +0x68C, as MissingShip generates
// it for its SoH fork (ovl_Ml_TreeBridge/z_ml_treebridge_col.inc there), in Unbound's widened layout: s32 vertices
// and an unpacked surface type. Polys keep their vertex indices; none has exclusion or conveyor flags.
static Vec3i sMlTreeBridgeVtx[] = {
    { 53, 0, 34 },
    { 36, 13, 16 },
    { 32, 27, 22 },
    { 28, 13, 29 },
    { -2, 0, 2 },
    { -37, 27, 22 },
    { -33, 13, 29 },
    { -41, 13, 16 },
    { -58, 0, 34 },
    { -2, 0, -62 },
    { -10, 13, -38 },
    { -2, 27, -38 },
    { 6, 13, -38 },
    { -4, 200, 12 },
    { -2, 277, 2 },
    { -26, 200, -22 },
    { -2, -70, 2 },
    { 12, 200, 12 },
    { 12, 200, -12 },
};
static CollisionPoly sMlTreeBridgePolys[] = {
    { 0x0000, { { 0x0000, 0x0001, 0x0002 } }, { 25939, 13677, -14620 }, -27 },
    { 0x0000, { { 0x0003, 0x0000, 0x0002 } }, { 1538, 14285, 29449 }, -33 },
    { 0x0000, { { 0x0004, 0x0003, 0x0002 } }, { -22745, 15422, 17847 }, -2 },
    { 0x0000, { { 0x0004, 0x0002, 0x0001 } }, { 5858, 14099, -28993 }, 2 },
    { 0x0000, { { 0x0004, 0x0005, 0x0006 } }, { 22380, 15509, 18229 }, 0 },
    { 0x0000, { { 0x0004, 0x0007, 0x0005 } }, { -5728, 14078, -29029 }, 1 },
    { 0x0000, { { 0x0007, 0x0008, 0x0005 } }, { -25939, 13677, -14620 }, -31 },
    { 0x0000, { { 0x0008, 0x0006, 0x0005 } }, { -1538, 14285, 29449 }, -33 },
    { 0x0000, { { 0x0009, 0x000A, 0x000B } }, { -24842, 14195, -15970 }, -32 },
    { 0x0000, { { 0x000C, 0x0009, 0x000B } }, { 24842, 14195, -15970 }, -29 },
    { 0x0000, { { 0x0004, 0x000C, 0x000B } }, { 26977, 15415, 10405 }, 1 },
    { 0x0000, { { 0x0004, 0x000B, 0x000A } }, { -26977, 15415, 10405 }, -2 },
    { 0x0000, { { 0x000D, 0x000E, 0x000F } }, { -27394, 3014, 17725 }, -28 },
    { 0x0000, { { 0x000D, 0x000F, 0x0010 } }, { -27501, -863, 17795 }, -5 },
    { 0x0000, { { 0x0011, 0x000E, 0x000D } }, { 0, 4220, 32494 }, -38 },
    { 0x0000, { { 0x0010, 0x0011, 0x000D } }, { 0, -1213, 32745 }, -5 },
    { 0x0000, { { 0x0012, 0x000E, 0x0011 } }, { 32238, 5862, 0 }, -48 },
    { 0x0000, { { 0x0012, 0x0011, 0x0010 } }, { 32723, -1697, 0 }, -2 },
    { 0x0000, { { 0x000F, 0x000E, 0x0012 } }, { 8141, 7105, -30934 }, -58 },
    { 0x0000, { { 0x0010, 0x000F, 0x0012 } }, { 8322, -2071, -31625 }, -2 },
    { 0x0000, { { 0x0003, 0x0001, 0x0000 } }, { -13329, -28788, -8203 }, 30 },
    { 0x0000, { { 0x0003, 0x0004, 0x0001 } }, { 8684, -31140, 5344 }, 0 },
    { 0x0000, { { 0x0007, 0x0006, 0x0008 } }, { 13329, -28788, -8203 }, 32 },
    { 0x0000, { { 0x0007, 0x0004, 0x0006 } }, { -8519, -31203, 5242 }, -1 },
    { 0x0000, { { 0x000A, 0x000C, 0x0004 } }, { 0, -31163, -10128 }, 1 },
    { 0x0000, { { 0x000A, 0x0009, 0x000C } }, { 0, -28812, 15606 }, 30 },
};
// ML's packed words 0x00000000 0x0000000A (SurfaceType_Unpack): material 10, everything else 0
static SurfaceType sMlTreeBridgeSurfaceTypes[] = {
    { .material = 10 },
};
// One camera entry that sets nothing, where ML has no camera data. A NULL list crashes stock Unbound's camera when
// Link stands on the bridge (z_bgcheck.c's func_80041A4C reads it unchecked; MissingShip's branch guards it, row B1);
// this entry gives those functions the values the guard returns: no setting, no cameras, no positions.
static CamData sMlTreeBridgeCamData[] = {
    { 0, 0, NULL },
};
static CollisionHeader sMlTreeBridgeCol = {
    { -158, -170, -162 }, { 153, 377, 134 },
    ARRAY_COUNT(sMlTreeBridgeVtx), sMlTreeBridgeVtx, ARRAY_COUNT(sMlTreeBridgePolys), sMlTreeBridgePolys,
    sMlTreeBridgeSurfaceTypes, sMlTreeBridgeCamData, 0, NULL, ARRAY_COUNT(sMlTreeBridgeCamData),
};
