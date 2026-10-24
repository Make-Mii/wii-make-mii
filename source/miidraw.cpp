#include <stdlib.h>
#include <mii.h>
#include <grrlib.h>
#include <stdio.h>
#include <ogc/isfs.h>
#include <malloc.h>
#include <string.h>

#include "mii_heads_png.h"
#include "mii_hairs1_png.h"
#include "mii_hairs2_png.h"
#include "mii_eyebrows_png.h"
#include "mii_eyes1_png.h"
#include "mii_eyes2_png.h"
#include "mii_eyes3_png.h"
#include "mii_noses_png.h"
#include "mii_features_png.h"
#include "mii_lips_png.h"
#include "mii_mole_png.h"
#include "mii_beards_png.h"
#include "mii_mustache_png.h"
#include "mii_glasses_png.h"

extern "C" int NoOfMiis;
Mii* miis;

static GRRLIB_texImg* img_heads;
static GRRLIB_texImg* img_hairs1;
static GRRLIB_texImg* img_hairs2;
static GRRLIB_texImg* img_eyebrows;
static GRRLIB_texImg* img_eyes1;
static GRRLIB_texImg* img_eyes2;
static GRRLIB_texImg* img_eyes3;
static GRRLIB_texImg* img_noses;
static GRRLIB_texImg* img_features;
static GRRLIB_texImg* img_lips;
static GRRLIB_texImg* img_mole;
static GRRLIB_texImg* img_beards;
static GRRLIB_texImg* img_mustache;
static GRRLIB_texImg* img_glasses;
GRRLIB_texImg* img_staticmii[100];

// ************************ DEFINE OUR ARRAYS ********************
int hairbg[72] = {56,56,56,56,56,56,56,56,56,56,56,56,16,56,56,56,56,56,17,18,56,19,20,56,56,56,21,56,56,56,56,56,56,56,56,56,22,23,56,56,24,25,56,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,56,56,48,49,50,51,52,53,56};
int hairfg[72] = {59,42,65,49,40,44,52,47,45,63,51,54,36,37,48,70,61,56,64,43,53,58,50,27,69,41,39,46,66,71,33,11,12,0,35,57,30,14,25,4,1,31,26,24,3,6,62,13,15,7,19,2,17,67,29,20,9,34,18,8,22,60,23,55,21,32,16,28,10,38,5,68};
u32 haircol[8] = {0x111111FF, 0x332222FF, 0x441111FF, 0xBB6644FF, 0x8888AAFF, 0x443322FF, 0x996644FF, 0xDDBB99FF};
u32 skincol[6] = {0xECCFBDFF, 0xF7BC7dFF, 0xD78A48FF, 0xF5B189FF, 0x995122FF, 0x563010FF};
u32 eyecol[6] = {0x000000FF, 0x778887FF, 0x7E6355FF, 0x888940FF, 0x6A84D0FF, 0x409B5AFF};
u32 lipcol[3] = {0xC76C46FF, 0xE44E3AFF, 0xD88789FF};
u32 glassescol[6] = {0x626D6CFF, 0x85703AFF, 0xAB4E37FF, 0x426996FF, 0xB97F27FF, 0xBDBFB9FF};
int eyebrows[24] = {1,3,14,15,11,10,0,6,8,4,13,12,2,19,16,18,22,9,21,5,17,7,20,23};
int eyes[48] = {2,6,0,42,1,24,29,36,3,16,45,13,17,26,46,9,8,5,33,14,11,20,44,18,30,21,7,10,34,41,31,32,15,12,19,23,27,28,38,4,22,25,39,43,37,40,35,47};
int noses[12] = {5,0,2,3,7,6,4,10,8,9,1,11};
int lips[24] = {6,1,14,16,17,5,10,12,7,13,8,19,23,11,22,18,9,15,21,2,20,3,4,0};
// ***************************************************************

// ************** DEFINE SOME VARIABLES FOR PROG USE *************
int feature=0;
int blink[100];
int blinkrand[100];
int blinkcount[100];
int startblink[100];

int neck=0;
int bodyadjust[100];
char miinames[100][11];
// ***************************************************************

static inline int safe_tile(int tile) {
    return (tile < 0) ? 0 : tile;
}

int MakeMiiHappy(int bodypart) {
    if(bodypart==1) {
        int miipart[8] = {3, 4, 5, 8, 14, 16, 19, 23};
        return(miipart[rand() % 8]);
    } else if(bodypart==2) {
        return 0;
    }
    return 0;
}

int MakeMiiSad(int bodypart) {
    if(bodypart==1) {
        int miipart[3] = {9, 22, 24};
        return(miipart[rand() % 3]);
    } else if(bodypart==2) {
        int miipart[3] = {6, 33, 39};
        return(miipart[rand() % 3]);
    }
    return 0;
}

int MakeMiiShocked(int bodypart) {
    if(bodypart==1) {
        int miipart[2] = {15, 11};
        return (miipart[rand() % 2]);
    } else if(bodypart==2) {
        int miipart[5] = {10, 13, 36, 37, 43};
        return(miipart[rand() % 5]);
    }
    return 0;
}

int MakeMiiMad(int bodypart) {
    if(bodypart==1) {
        int miipart[3] = {1, 21, 24};
        return (miipart[rand() % 3]);
    } else if(bodypart==2) {
        return 44;
    }
    return 0;
}

int  rawMiiCount = -1;      // -1 = could not read the file
char rawName0[11] = "";

// Read n bits, MSB first, starting at absolute bit offset 'bitoff' of a record
static u32 getbits(const u8 *rec, int bitoff, int n) {
    u32 v = 0;
    for (int i = 0; i < n; i++) {
        int b = bitoff + i;
        v = (v << 1) | ((rec[b >> 3] >> (7 - (b & 7))) & 1);
    }
    return v;
}
#define FLD(addr, off, n) getbits(rec, (addr) * 8 + (off), (n))

static void FixMiisFromRaw() {
    if (miis == NULL || NoOfMiis <= 0) return;

    const int RAW_SIZE = 7424;
    u8 *buf = (u8*)memalign(32, RAW_SIZE);
    if (!buf) return;
    memset(buf, 0, RAW_SIZE);

    ISFS_Initialize();
    s32 fd = ISFS_Open("/shared2/menu/FaceLib/RFL_DB.dat", ISFS_OPEN_READ);
    if (fd < 0) { free(buf); return; }
    s32 r = ISFS_Read(fd, buf, RAW_SIZE);
    ISFS_Close(fd);
    if (r < 4 + 74) { free(buf); return; }

    int found = 0;
    for (int slot = 0; slot < 100 && found < NoOfMiis; slot++) {
        const u8 *rec = buf + 4 + slot * 74;
        if (rec[2] == 0 && rec[3] == 0) continue;          // empty name = empty slot

        Mii &m = miis[found];

        m.faceShape           = FLD(0x20, 0, 3);
        m.skinColor           = FLD(0x20, 3, 3);
        m.facialFeature       = FLD(0x20, 6, 4);

        m.hairType            = FLD(0x22, 0, 7);
        m.hairColor           = FLD(0x22, 7, 3);
        m.hairPart            = FLD(0x22, 10, 1);

        m.eyebrowType         = FLD(0x24, 0, 5);
        m.eyebrowRotation     = FLD(0x24, 6, 4);
        m.eyebrowColor        = FLD(0x24, 16, 3);
        m.eyebrowSize         = FLD(0x24, 19, 4);
        m.eyebrowVertPos      = FLD(0x24, 23, 5);
        m.eyebrowHorizSpacing = FLD(0x24, 28, 4);

        m.eyeType             = FLD(0x28, 0, 6);
        m.eyeRotation         = FLD(0x28, 8, 3);
        m.eyeVertPos          = FLD(0x28, 11, 5);
        m.eyeColor            = FLD(0x28, 16, 3);
        m.eyeSize             = FLD(0x28, 20, 3);
        m.eyeHorizSpacing     = FLD(0x28, 23, 4);

        m.noseType            = FLD(0x2C, 0, 4);
        m.noseSize            = FLD(0x2C, 4, 4);
        m.noseVertPos         = FLD(0x2C, 8, 5);

        m.lipType             = FLD(0x2E, 0, 5);
        m.lipColor            = FLD(0x2E, 5, 2);
        m.lipSize             = FLD(0x2E, 7, 4);
        m.lipVertPos          = FLD(0x2E, 11, 5);

        m.glassesType         = FLD(0x30, 0, 4);
        m.glassesColor        = FLD(0x30, 4, 3);
        m.glassesSize         = FLD(0x30, 8, 3);
        m.glassesVertPos      = FLD(0x30, 11, 5);

        m.mustacheType        = FLD(0x32, 0, 2);
        m.beardType           = FLD(0x32, 2, 2);
        m.facialHairColor     = FLD(0x32, 4, 3);
        m.mustacheSize        = FLD(0x32, 7, 4);
        m.mustacheVertPos     = FLD(0x32, 11, 5);

        m.mole                = FLD(0x34, 0, 1);
        m.moleSize            = FLD(0x34, 1, 4);
        m.moleVertPos         = FLD(0x34, 5, 5);
        m.moleHorizPos        = FLD(0x34, 10, 5);

        if (found == 0) {
            for (int i = 0; i < 10; i++) {
                unsigned char hi = rec[2 + i * 2], lo = rec[3 + i * 2];
                rawName0[i] = (hi == 0 && lo >= 32 && lo < 127) ? (char)lo : (lo ? '?' : '\0');
            }
            rawName0[10] = '\0';
        }
        found++;
    }
    rawMiiCount = found;
    free(buf);
}

void GRRLIB_MiisInit() {
    img_heads = GRRLIB_LoadTexture(mii_heads_png);
    img_hairs1 = GRRLIB_LoadTexture(mii_hairs1_png);
    img_hairs2 = GRRLIB_LoadTexture(mii_hairs2_png);
    img_eyebrows = GRRLIB_LoadTexture(mii_eyebrows_png);
    img_eyes1 = GRRLIB_LoadTexture(mii_eyes1_png);
    img_eyes2 = GRRLIB_LoadTexture(mii_eyes2_png);
    img_eyes3 = GRRLIB_LoadTexture(mii_eyes3_png);
    img_noses = GRRLIB_LoadTexture(mii_noses_png);
    img_features = GRRLIB_LoadTexture(mii_features_png);
    img_lips = GRRLIB_LoadTexture(mii_lips_png);
    img_mole = GRRLIB_LoadTexture(mii_mole_png);
    img_beards = GRRLIB_LoadTexture(mii_beards_png);
    img_mustache = GRRLIB_LoadTexture(mii_mustache_png);
    img_glasses = GRRLIB_LoadTexture(mii_glasses_png);

    GRRLIB_InitTileSet(img_heads, 120, 120, 0);
    GRRLIB_InitTileSet(img_hairs1, 120, 120, 0);
    GRRLIB_InitTileSet(img_hairs2, 120, 120, 0);
    GRRLIB_InitTileSet(img_eyebrows, 36, 54, 0);
    GRRLIB_InitTileSet(img_eyes1, 54, 54, 0);
    GRRLIB_InitTileSet(img_eyes2, 54, 54, 0);
    GRRLIB_InitTileSet(img_eyes3, 54, 54, 0);
    GRRLIB_InitTileSet(img_noses, 50, 50, 0);
    GRRLIB_InitTileSet(img_features, 120, 120, 0);
    GRRLIB_InitTileSet(img_lips, 60, 60, 0);
    GRRLIB_InitTileSet(img_beards, 120, 140, 0);
    GRRLIB_InitTileSet(img_mustache, 60, 60, 0);
    GRRLIB_InitTileSet(img_glasses, 180, 72, 0);

    miis = loadMiis_Wii();    
	FixMiisFromRaw();
}

static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

void GRRLIB_BuildMii(Mii mii, int miiid, int miiblinks, int miieyes, int miimouth, GRRLIB_texImg* tex) {
    if (tex == NULL || miiid < 0 || miiid >= 100) return;

    mii.hairPart            = clampi(mii.hairPart, 0, 1);
    mii.eyeSize             = clampi(mii.eyeSize, 0, 7);
    mii.eyeVertPos          = clampi(mii.eyeVertPos, 0, 18);
    mii.eyeHorizSpacing     = clampi(mii.eyeHorizSpacing, 0, 12);
    mii.eyeRotation         = clampi(mii.eyeRotation, 0, 7);
    mii.eyebrowSize         = clampi(mii.eyebrowSize, 0, 8);
    mii.eyebrowVertPos      = clampi(mii.eyebrowVertPos, 3, 18);
    mii.eyebrowHorizSpacing = clampi(mii.eyebrowHorizSpacing, 0, 12);
    mii.eyebrowRotation     = clampi(mii.eyebrowRotation, 0, 11);
    mii.noseSize            = clampi(mii.noseSize, 0, 8);
    mii.noseVertPos         = clampi(mii.noseVertPos, 0, 18);
    mii.lipSize             = clampi(mii.lipSize, 0, 8);
    mii.lipVertPos          = clampi(mii.lipVertPos, 0, 18);
    mii.beardType           = clampi(mii.beardType, 0, 3);
    mii.mustacheType        = clampi(mii.mustacheType, 0, 3);
    mii.mustacheSize        = clampi(mii.mustacheSize, 0, 8);
    mii.mustacheVertPos     = clampi(mii.mustacheVertPos, 0, 16);
    mii.glassesSize         = clampi(mii.glassesSize, 0, 7);
    mii.glassesVertPos      = clampi(mii.glassesVertPos, 0, 20);
    mii.mole                = clampi(mii.mole, 0, 1);
    mii.moleSize            = clampi(mii.moleSize, 0, 8);
    mii.moleVertPos         = clampi(mii.moleVertPos, 0, 30);
    mii.moleHorizPos        = clampi(mii.moleHorizPos, 0, 16);
    mii.facialFeature       = clampi(mii.facialFeature, 0, 11);

    // Clamp Mii indices to valid array bounds
    int hType = (mii.hairType >= 0 && mii.hairType < 72) ? mii.hairType : 0;
    int hCol  = (mii.hairColor >= 0 && mii.hairColor < 8) ? mii.hairColor : 0;
    int sCol  = (mii.skinColor >= 0 && mii.skinColor < 6) ? mii.skinColor : 0;
    int eCol  = (mii.eyeColor >= 0 && mii.eyeColor < 6) ? mii.eyeColor : 0;
    int fShape = (mii.faceShape >= 0 && mii.faceShape < 8) ? mii.faceShape : 0;
    int eType = (mii.eyeType >= 0 && mii.eyeType < 48) ? mii.eyeType : 0;
    int ebType = (mii.eyebrowType >= 0 && mii.eyebrowType < 24) ? mii.eyebrowType : 0;
    int ebCol = (mii.eyebrowColor >= 0 && mii.eyebrowColor < 8) ? mii.eyebrowColor : 0;
    int nType = (mii.noseType >= 0 && mii.noseType < 12) ? mii.noseType : 0;
    int lType = (mii.lipType >= 0 && mii.lipType < 24) ? mii.lipType : 0;
    int lCol  = (mii.lipColor >= 0 && mii.lipColor < 3) ? mii.lipColor : 0;
    int gType = (mii.glassesType >= 0 && mii.glassesType < 9) ? mii.glassesType : 0;
    int gCol  = (mii.glassesColor >= 0 && mii.glassesColor < 6) ? mii.glassesColor : 0;

	GRRLIB_CompoStart();
    GRRLIB_FillScreen(0x00000000); // Clear composition buffer to transparent black

    // 1. Back Hair
    GRRLIB_SetHandle(img_hairs1, 60, 0);
    GRRLIB_SetHandle(img_hairs2, 60, 0);
    int bgTile = hairbg[hairfg[hType]];
    GRRLIB_DrawTile(230, 210, img_hairs2, 0, -1*(2*mii.hairPart-1), 1, haircol[hCol], safe_tile(bgTile));

    // 2. Head Shape
    GRRLIB_DrawTile(230, 200, img_heads, 0, 1, 1, skincol[sCol], safe_tile(fShape));

    // 3. Neck mapping
    if(fShape==0) neck=2;
    else if(fShape==1) neck=0;
    else if(fShape==2) neck=8;
    else if(fShape==3) neck=5;
    else if(fShape==4) neck=1;
    else if(fShape==5) neck=1;
    else if(fShape==6) neck=9;
    else if(fShape==7) neck=10;

    // 4. Facial Features
    if(mii.facialFeature==0) feature=39;
    else if(mii.facialFeature==1 || mii.facialFeature==2) feature=fShape;
    else if(mii.facialFeature==3) feature=33;
    else if(mii.facialFeature==4) feature=34;
    else if(mii.facialFeature==5) feature=35;
    else if(mii.facialFeature==6) feature=36;
    else if(mii.facialFeature==7) feature=40+fShape;
    else if(mii.facialFeature==8) feature=38;
    else if(mii.facialFeature==9) feature=8+fShape;
    else if(mii.facialFeature==10) feature=16+fShape;
    else if(mii.facialFeature==11) feature=24+fShape;

    if(mii.facialFeature==2) {
        GRRLIB_DrawTile(230, 200, img_features, 0, 1, 1, skincol[sCol], 32);
    }
    GRRLIB_DrawTile(230, 200, img_features, 0, 1, 1, skincol[sCol], safe_tile(feature));

    // 5. Beard
    if(mii.beardType > 0 && mii.beardType <= 4) {
        int fhCol = (mii.facialHairColor >= 0 && mii.facialHairColor < 8) ? mii.facialHairColor : 0;
        GRRLIB_DrawTile(230, 200, img_beards, 0, 1, 1, haircol[fhCol], safe_tile(((mii.beardType-1)*8)+fShape));
    }

    // 6. Mole
    GRRLIB_SetHandle(img_mole, 6, 6);
    if(mii.mole == 1) {
        GRRLIB_DrawImg(252+(4*mii.moleHorizPos), 212+(2.8*mii.moleVertPos), img_mole, 0, .2+(mii.moleSize*0.1), .2+(mii.moleSize*0.1), 0xFFFFFFFF);
    }

    // 7. Blinking state update
    blink[miiid]++;
    if(blink[miiid] > 300 + 100*blinkrand[miiid]) startblink[miiid]=1;
    if(startblink[miiid] == 1) blinkcount[miiid]++;
    if(blinkcount[miiid] > 11) {
        blinkrand[miiid] = rand() % 8;
        blink[miiid] = 0;
        startblink[miiid] = 0;
        blinkcount[miiid] = 0;
    }

    // 8. Eyes
    if(blinkcount[miiid] < 11 && blinkcount[miiid] > 1 && miiblinks == 1) {
        GRRLIB_SetHandle(img_eyes1, 18, 36);
        GRRLIB_DrawTile(272+(2.6*mii.eyeHorizSpacing), 184+(2.8*(mii.eyeVertPos)), img_eyes1, 11.25 * (7-mii.eyeRotation), .3+(mii.eyeSize*0.1), .3+(mii.eyeSize*0.1), 0xFFFFFFFF, 46);
        GRRLIB_SetHandle(img_eyes2, 18, 36);
        GRRLIB_DrawTile(272+(2.6*mii.eyeHorizSpacing), 184+(2.8*(mii.eyeVertPos)), img_eyes2, 11.25 * (7-mii.eyeRotation), .3+(mii.eyeSize*0.1), .3+(mii.eyeSize*0.1), eyecol[eCol], 46);
        GRRLIB_SetHandle(img_eyes3, 18, 36);
        GRRLIB_DrawTile(272+(2.6*mii.eyeHorizSpacing), 184+(2.8*(mii.eyeVertPos)), img_eyes3, 11.25 * (7-mii.eyeRotation), .3+(mii.eyeSize*0.1), .3+(mii.eyeSize*0.1), 0xFFFFFFFF, 46);

        GRRLIB_SetHandle(img_eyes1, 36, 36);
        GRRLIB_DrawTile(254-(2.6*mii.eyeHorizSpacing), 184+(2.8*(mii.eyeVertPos)), img_eyes1, -11.25 * (7-mii.eyeRotation), .3+(mii.eyeSize*0.1), .3+(mii.eyeSize*0.1), 0xFFFFFFFF, safe_tile(95-46));
        GRRLIB_SetHandle(img_eyes2, 36, 36);
        GRRLIB_DrawTile(254-(2.6*mii.eyeHorizSpacing), 184+(2.8*(mii.eyeVertPos)), img_eyes2, -11.25 * (7-mii.eyeRotation), .3+(mii.eyeSize*0.1), .3+(mii.eyeSize*0.1), eyecol[eCol], safe_tile(95-46));
        GRRLIB_SetHandle(img_eyes3, 36, 36);
        GRRLIB_DrawTile(254-(2.6*mii.eyeHorizSpacing), 184+(2.8*(mii.eyeVertPos)), img_eyes3, -11.25 * (7-mii.eyeRotation), .3+(mii.eyeSize*0.1), .3+(mii.eyeSize*0.1), 0xFFFFFFFF, safe_tile(95-46));
    } else {
        int selectedEye = (miieyes == 0) ? eyes[eType] : (miieyes - 1);
        selectedEye = safe_tile(selectedEye);

        GRRLIB_SetHandle(img_eyes1, 18, 36);
        GRRLIB_DrawTile(272+(2.6*mii.eyeHorizSpacing), 184+(2.8*(mii.eyeVertPos)), img_eyes1, 11.25 * (7-mii.eyeRotation), .3+(mii.eyeSize*0.1), .3+(mii.eyeSize*0.1), 0xFFFFFFFF, selectedEye);
        GRRLIB_SetHandle(img_eyes2, 18, 36);
        GRRLIB_DrawTile(272+(2.6*mii.eyeHorizSpacing), 184+(2.8*(mii.eyeVertPos)), img_eyes2, 11.25 * (7-mii.eyeRotation), .3+(mii.eyeSize*0.1), .3+(mii.eyeSize*0.1), eyecol[eCol], selectedEye);
        GRRLIB_SetHandle(img_eyes3, 18, 36);
        GRRLIB_DrawTile(272+(2.6*mii.eyeHorizSpacing), 184+(2.8*(mii.eyeVertPos)), img_eyes3, 11.25 * (7-mii.eyeRotation), .3+(mii.eyeSize*0.1), .3+(mii.eyeSize*0.1), 0xFFFFFFFF, selectedEye);

        GRRLIB_SetHandle(img_eyes1, 36, 36);
        GRRLIB_DrawTile(254-(2.6*mii.eyeHorizSpacing), 184+(2.8*(mii.eyeVertPos)), img_eyes1, -11.25 * (7-mii.eyeRotation), .3+(mii.eyeSize*0.1), .3+(mii.eyeSize*0.1), 0xFFFFFFFF, safe_tile(95-selectedEye));
        GRRLIB_SetHandle(img_eyes2, 36, 36);
        GRRLIB_DrawTile(254-(2.6*mii.eyeHorizSpacing), 184+(2.8*(mii.eyeVertPos)), img_eyes2, -11.25 * (7-mii.eyeRotation), .3+(mii.eyeSize*0.1), .3+(mii.eyeSize*0.1), eyecol[eCol], safe_tile(95-selectedEye));
        GRRLIB_SetHandle(img_eyes3, 36, 36);
        GRRLIB_DrawTile(254-(2.6*mii.eyeHorizSpacing), 184+(2.8*(mii.eyeVertPos)), img_eyes3, -11.25 * (7-mii.eyeRotation), .3+(mii.eyeSize*0.1), .3+(mii.eyeSize*0.1), 0xFFFFFFFF, safe_tile(95-selectedEye));
    }

    // 9. Eyebrows
    int ebTile = eyebrows[ebType];
    GRRLIB_SetHandle(img_eyebrows, 0, 54);
    GRRLIB_DrawTile(288+(2.6*mii.eyebrowHorizSpacing), 166+(2.8*(mii.eyebrowVertPos-3)), img_eyebrows, 11.25 * (11-mii.eyebrowRotation), .3+(mii.eyebrowSize*0.1), .3+(mii.eyebrowSize*0.1), haircol[ebCol], safe_tile(ebTile));
    GRRLIB_SetHandle(img_eyebrows, 36, 54);
    GRRLIB_DrawTile(256-(2.6*mii.eyebrowHorizSpacing), 166+(2.8*(mii.eyebrowVertPos-3)), img_eyebrows, -11.25 * (11-mii.eyebrowRotation), .3+(mii.eyebrowSize*0.1), .3+(mii.eyebrowSize*0.1), haircol[ebCol], safe_tile(53-ebTile));

    // 10. Lips
    GRRLIB_SetHandle(img_lips, 30, 30);
    int lipTile = (miimouth == 0) ? lips[lType] : (miimouth - 1);
    lipTile = safe_tile(lipTile);
    u32 finalLipCol = 0xFFFFFFFF;
    if (miimouth == 0) {
        if(lips[lType]==1 || lips[lType]==6 || lips[lType]==11 || lips[lType]==17 || lips[lType]==19)
            finalLipCol = lipcol[lCol];
    } else {
        if(lipTile==1 || lipTile==6 || lipTile==11 || lipTile==17 || lipTile==19)
            finalLipCol = 0xC76C46FF;
    }
    GRRLIB_DrawTile(260, 220+(2.6*mii.lipVertPos), img_lips, 0, .2+(mii.lipSize*0.1), .2+(mii.lipSize*0.1), finalLipCol, lipTile);

    // 11. Mustache
    if(mii.mustacheType > 0 && mii.mustacheType <= 4) {
        int fhCol = (mii.facialHairColor >= 0 && mii.facialHairColor < 8) ? mii.facialHairColor : 0;
        GRRLIB_SetHandle(img_mustache, 30, 10);
        GRRLIB_DrawTile(260, 244+(2.9*mii.mustacheVertPos), img_mustache, 0, .2+(mii.mustacheSize*0.1), .2+(mii.mustacheSize*0.1), haircol[fhCol], safe_tile(mii.mustacheType-1));
    }

    // 12. Nose
    GRRLIB_SetHandle(img_noses, 25, 30);
    GRRLIB_DrawTile(265, 220+(2.6*mii.noseVertPos), img_noses, 0, .2+(mii.noseSize*0.1), .2+(mii.noseSize*0.1), skincol[sCol], safe_tile(noses[nType]));

    // 13. Front Hair
    int fgTile = hairfg[hType];
    if(fgTile < 56) {
        GRRLIB_DrawTile(230, 190, img_hairs1, 0, -1*(2*mii.hairPart-1), 1, haircol[hCol], safe_tile(fgTile));
    } else {
        GRRLIB_DrawTile(230, 190, img_hairs2, 0, -1*(2*mii.hairPart-1), 1, haircol[hCol], safe_tile(fgTile - 56));
    }

    // 14. Glasses
    if(gType > 0) {
        GRRLIB_SetHandle(img_glasses, 90, 32);
        if(gType < 6) {
            GRRLIB_DrawTile(200, 195+(2.6*(mii.glassesVertPos)), img_glasses, 0, .1+(mii.glassesSize*0.1), .1+(mii.glassesSize*0.1), glassescol[gCol], safe_tile(gType-1));
        } else {
            GRRLIB_DrawTile(200, 195+(2.6*(mii.glassesVertPos)), img_glasses, 0, .1+(mii.glassesSize*0.1), .1+(mii.glassesSize*0.1), glassescol[gCol], safe_tile(gType+2));
            GRRLIB_DrawTile(200, 195+(2.6*(mii.glassesVertPos)), img_glasses, 0, .1+(mii.glassesSize*0.1), .1+(mii.glassesSize*0.1), 0xFFFFFFFF, safe_tile(gType-1));
        }
    }

    GRRLIB_CompoEnd(200, 160, tex);

    // Save name (printable ASCII only) and body adjustments
    int k = 0;
    for (; k < 10 && mii.name[k]; k++) {
        unsigned char c = (unsigned char)mii.name[k];
        miinames[miiid][k] = (c >= 32 && c < 127) ? (char)c : '?';
    }
    miinames[miiid][k] = '\0';
    if (k == 0) snprintf(miinames[miiid], 11, "Mii %d", miiid + 1);
    bodyadjust[miiid] = neck;
}

void GRRLIB_CreateStaticMiis() {
    if (miis == NULL || NoOfMiis <= 0) return;

    for (int i = 0; i < NoOfMiis && i < 100; i++) {
        img_staticmii[i] = GRRLIB_CreateEmptyTexture(180, 200);
        if (img_staticmii[i] != NULL) {
            GRRLIB_BuildMii(miis[i], i, 0, 0, 0, img_staticmii[i]);
        }
    }
}