#include "miirender.h"
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogc/isfs.h>

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

// ---------------------------------------------------------------- sprite sheets
static GRRLIB_texImg *s_heads, *s_hairs1, *s_hairs2, *s_eyebrows, *s_eyes1, *s_eyes2, *s_eyes3,
                     *s_noses, *s_features, *s_lips, *s_mole, *s_beards, *s_mustache, *s_glasses;
static bool s_ready = false;

// ---------------------------------------------------------------- lookup tables
static const int kHairBg[72] = {56,56,56,56,56,56,56,56,56,56,56,56,16,56,56,56,56,56,17,18,56,19,20,56,56,56,21,56,56,56,56,56,56,56,56,56,22,23,56,56,24,25,56,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,56,56,48,49,50,51,52,53,56};
static const int kHairFg[72] = {59,42,65,49,40,44,52,47,45,63,51,54,36,37,48,70,61,56,64,43,53,58,50,27,69,41,39,46,66,71,33,11,12,0,35,57,30,14,25,4,1,31,26,24,3,6,62,13,15,7,19,2,17,67,29,20,9,34,18,8,22,60,23,55,21,32,16,28,10,38,5,68};
static const u32 kHairCol[8]    = {0x111111FF,0x332222FF,0x441111FF,0xBB6644FF,0x8888AAFF,0x443322FF,0x996644FF,0xDDBB99FF};
static const u32 kSkinCol[6]    = {0xECCFBDFF,0xF7BC7DFF,0xD78A48FF,0xF5B189FF,0x995122FF,0x563010FF};
static const u32 kEyeCol[6]     = {0x000000FF,0x778887FF,0x7E6355FF,0x888940FF,0x6A84D0FF,0x409B5AFF};
static const u32 kLipCol[3]     = {0xC76C46FF,0xE44E3AFF,0xD88789FF};
static const u32 kGlassesCol[6] = {0x626D6CFF,0x85703AFF,0xAB4E37FF,0x426996FF,0xB97F27FF,0xBDBFB9FF};
static const int kEyebrows[24]  = {1,3,14,15,11,10,0,6,8,4,13,12,2,19,16,18,22,9,21,5,17,7,20,23};
static const int kEyes[48]      = {2,6,0,42,1,24,29,36,3,16,45,13,17,26,46,9,8,5,33,14,11,20,44,18,30,21,7,10,34,41,31,32,15,12,19,23,27,28,38,4,22,25,39,43,37,40,35,47};
static const int kNoses[12]     = {5,0,2,3,7,6,4,10,8,9,1,11};
static const int kLips[24]      = {6,1,14,16,17,5,10,12,7,13,8,19,23,11,22,18,9,15,21,2,20,3,4,0};

static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline int validIdx(int v, int n)        { return (v >= 0 && v < n) ? v : 0; }
static inline int tileOf(int t)                 { return t < 0 ? 0 : t; }

// ---------------------------------------------------------------- init / shutdown
bool MiiRender_Init() {
    s_heads    = GRRLIB_LoadTexture(mii_heads_png);
    s_hairs1   = GRRLIB_LoadTexture(mii_hairs1_png);
    s_hairs2   = GRRLIB_LoadTexture(mii_hairs2_png);
    s_eyebrows = GRRLIB_LoadTexture(mii_eyebrows_png);
    s_eyes1    = GRRLIB_LoadTexture(mii_eyes1_png);
    s_eyes2    = GRRLIB_LoadTexture(mii_eyes2_png);
    s_eyes3    = GRRLIB_LoadTexture(mii_eyes3_png);
    s_noses    = GRRLIB_LoadTexture(mii_noses_png);
    s_features = GRRLIB_LoadTexture(mii_features_png);
    s_lips     = GRRLIB_LoadTexture(mii_lips_png);
    s_mole     = GRRLIB_LoadTexture(mii_mole_png);
    s_beards   = GRRLIB_LoadTexture(mii_beards_png);
    s_mustache = GRRLIB_LoadTexture(mii_mustache_png);
    s_glasses  = GRRLIB_LoadTexture(mii_glasses_png);

    s_ready = s_heads && s_hairs1 && s_hairs2 && s_eyebrows && s_eyes1 && s_eyes2 && s_eyes3 &&
              s_noses && s_features && s_lips && s_mole && s_beards && s_mustache && s_glasses;
    if (!s_ready) return false;

    GRRLIB_InitTileSet(s_heads,    120, 120, 0);
    GRRLIB_InitTileSet(s_hairs1,   120, 120, 0);
    GRRLIB_InitTileSet(s_hairs2,   120, 120, 0);
    GRRLIB_InitTileSet(s_eyebrows,  36,  54, 0);
    GRRLIB_InitTileSet(s_eyes1,     54,  54, 0);
    GRRLIB_InitTileSet(s_eyes2,     54,  54, 0);
    GRRLIB_InitTileSet(s_eyes3,     54,  54, 0);
    GRRLIB_InitTileSet(s_noses,     50,  50, 0);
    GRRLIB_InitTileSet(s_features, 120, 120, 0);
    GRRLIB_InitTileSet(s_lips,      60,  60, 0);
    GRRLIB_InitTileSet(s_beards,   120, 140, 0);
    GRRLIB_InitTileSet(s_mustache,  60,  60, 0);
    GRRLIB_InitTileSet(s_glasses,  180,  72, 0);
    return true;
}

void MiiRender_Shutdown() {
    GRRLIB_texImg **all[] = { &s_heads,&s_hairs1,&s_hairs2,&s_eyebrows,&s_eyes1,&s_eyes2,&s_eyes3,
                              &s_noses,&s_features,&s_lips,&s_mole,&s_beards,&s_mustache,&s_glasses };
    for (unsigned i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        if (*all[i]) { GRRLIB_FreeTexture(*all[i]); *all[i] = NULL; }
    }
    s_ready = false;
}

// ---------------------------------------------------------------- raw decoding
// Read n bits, MSB first, from absolute bit offset 'bitoff' of a record.
static u32 getbits(const u8 *rec, int bitoff, int n) {
    u32 v = 0;
    for (int i = 0; i < n; i++) {
        int b = bitoff + i;
        v = (v << 1) | ((rec[b >> 3] >> (7 - (b & 7))) & 1);
    }
    return v;
}
#define FLD(addr, off, n) getbits(rec74, (addr) * 8 + (off), (n))

void MiiDecodeRaw(const u8 *rec74, Mii *out) {
    memset(out, 0, sizeof(Mii));
    out->faceShape           = FLD(0x20, 0, 3);
    out->skinColor           = FLD(0x20, 3, 3);
    out->facialFeature       = FLD(0x20, 6, 4);

    out->hairType            = FLD(0x22, 0, 7);
    out->hairColor           = FLD(0x22, 7, 3);
    out->hairPart            = FLD(0x22, 10, 1);

    out->eyebrowType         = FLD(0x24, 0, 5);
    out->eyebrowRotation     = FLD(0x24, 6, 4);
    out->eyebrowColor        = FLD(0x24, 16, 3);
    out->eyebrowSize         = FLD(0x24, 19, 4);
    out->eyebrowVertPos      = FLD(0x24, 23, 5);
    out->eyebrowHorizSpacing = FLD(0x24, 28, 4);

    out->eyeType             = FLD(0x28, 0, 6);
    out->eyeRotation         = FLD(0x28, 8, 3);
    out->eyeVertPos          = FLD(0x28, 11, 5);
    out->eyeColor            = FLD(0x28, 16, 3);
    out->eyeSize             = FLD(0x28, 20, 3);
    out->eyeHorizSpacing     = FLD(0x28, 23, 4);

    out->noseType            = FLD(0x2C, 0, 4);
    out->noseSize            = FLD(0x2C, 4, 4);
    out->noseVertPos         = FLD(0x2C, 8, 5);

    out->lipType             = FLD(0x2E, 0, 5);
    out->lipColor            = FLD(0x2E, 5, 2);
    out->lipSize             = FLD(0x2E, 7, 4);
    out->lipVertPos          = FLD(0x2E, 11, 5);

    out->glassesType         = FLD(0x30, 0, 4);
    out->glassesColor        = FLD(0x30, 4, 3);
    out->glassesSize         = FLD(0x30, 8, 3);
    out->glassesVertPos      = FLD(0x30, 11, 5);

    out->mustacheType        = FLD(0x32, 0, 2);
    out->beardType           = FLD(0x32, 2, 2);
    out->facialHairColor     = FLD(0x32, 4, 3);
    out->mustacheSize        = FLD(0x32, 7, 4);
    out->mustacheVertPos     = FLD(0x32, 11, 5);

    out->mole                = FLD(0x34, 0, 1);
    out->moleSize            = FLD(0x34, 1, 4);
    out->moleVertPos         = FLD(0x34, 5, 5);
    out->moleHorizPos        = FLD(0x34, 10, 5);
}

int MiiLoadFromNand(Mii *out, int maxOut) {
    const int RAW_SIZE = 7424;                        // 4-byte header + 100 * 74, padded to 32
    u8 *buf = (u8 *)memalign(32, RAW_SIZE);
    if (!buf) return 0;
    memset(buf, 0, RAW_SIZE);

    ISFS_Initialize();
    s32 fd = ISFS_Open("/shared2/menu/FaceLib/RFL_DB.dat", ISFS_OPEN_READ);
    if (fd < 0) { free(buf); return 0; }
    s32 r = ISFS_Read(fd, buf, RAW_SIZE);
    ISFS_Close(fd);
    if (r < 4 + 74) { free(buf); return 0; }

    int found = 0;
    for (int slot = 0; slot < 100 && found < maxOut; slot++) {
        const u8 *rec = buf + 4 + slot * 74;
        if (rec[2] == 0 && rec[3] == 0) continue;     // empty name = empty slot
        MiiDecodeRaw(rec, &out[found++]);
    }
    free(buf);
    return found;
}

// ---------------------------------------------------------------- expressions
static int pick(const int *a, int n) { return a[rand() % n]; }

// ---------------------------------------------------------------- MiiAvatar
MiiAvatar::MiiAvatar()
    : tex_(GRRLIB_CreateEmptyTexture(180, 200)), eyes_(0), mouth_(0),
      dirty_(true), blinking_(true), closed_(false),
      blinkTimer_(0), blinkRand_(0), blinkCount_(0), blinkActive_(false) {
    memset(&mii_, 0, sizeof(mii_));
}

MiiAvatar::~MiiAvatar() {
    if (tex_) GRRLIB_FreeTexture(tex_);
}

void MiiAvatar::setMii(const Mii &mii) {
    mii_ = mii;
    dirty_ = true;
}

void MiiAvatar::setExpression(MiiExpression e) {
    static const int happy[8]   = {3, 4, 5, 8, 14, 16, 19, 23};
    static const int sad[3]     = {9, 22, 24};
    static const int shocked[2] = {15, 11};
    static const int mad[3]     = {1, 21, 24};
    eyes_ = 0;
    switch (e) {
        case MII_HAPPY:   mouth_ = pick(happy, 8);   break;
        case MII_SAD:     mouth_ = pick(sad, 3);     break;
        case MII_SHOCKED: mouth_ = pick(shocked, 2); break;
        case MII_MAD:     mouth_ = pick(mad, 3);     break;
        default:          mouth_ = 0;                break;
    }
    dirty_ = true;
}

void MiiAvatar::update() {
    bool closed = false;
    if (blinking_) {
        blinkTimer_++;
        if (blinkTimer_ > 300 + 100 * blinkRand_) blinkActive_ = true;
        if (blinkActive_) blinkCount_++;
        if (blinkCount_ > 11) {
            blinkRand_ = rand() % 8;
            blinkTimer_ = 0; blinkActive_ = false; blinkCount_ = 0;
        }
        closed = blinkActive_ && blinkCount_ > 1 && blinkCount_ < 11;
    }
    if (dirty_ || closed != closed_) {
        closed_ = closed;
        rebuild();
        dirty_ = false;
    }
}

void MiiAvatar::draw(f32 cx, f32 cy, f32 scale) const {
    if (!tex_) return;
    // The face is centred in the 180x200 texture; handle is (0,0), so scaling is about the centre.
    GRRLIB_DrawImg(cx - tex_->w * 0.5f, cy - tex_->h * 0.5f, tex_, 0, scale, scale, 0xFFFFFFFF);
}

static void drawEye(f32 x, f32 y, int handleX, f32 rot, f32 sc, int tile, u32 irisCol) {
    GRRLIB_texImg *sheets[3] = { s_eyes1, s_eyes2, s_eyes3 };
    for (int i = 0; i < 3; i++) {
        GRRLIB_SetHandle(sheets[i], handleX, 36);
        GRRLIB_DrawTile(x, y, sheets[i], rot, sc, sc, (i == 1) ? irisCol : 0xFFFFFFFF, tile);
    }
}

void MiiAvatar::rebuild() {
    if (!tex_ || !s_ready) return;
    const Mii &m = mii_;

    // Sanitised copies: nothing below can index out of range or scale wildly.
    const int hairType = validIdx(m.hairType, 72),  hairColor = validIdx(m.hairColor, 8);
    const int hairPart = clampi(m.hairPart, 0, 1);
    const int faceShape = validIdx(m.faceShape, 8), skinColor = validIdx(m.skinColor, 6);
    const int facialFeature = clampi(m.facialFeature, 0, 11);
    const int ebType = validIdx(m.eyebrowType, 24), ebColor = validIdx(m.eyebrowColor, 8);
    const int ebSize = clampi(m.eyebrowSize, 0, 8), ebV = clampi(m.eyebrowVertPos, 3, 18);
    const int ebH = clampi(m.eyebrowHorizSpacing, 0, 12), ebRot = clampi(m.eyebrowRotation, 0, 11);
    const int eyeType = validIdx(m.eyeType, 48), eyeColor = validIdx(m.eyeColor, 6);
    const int eyeSize = clampi(m.eyeSize, 0, 7), eyeV = clampi(m.eyeVertPos, 0, 18);
    const int eyeH = clampi(m.eyeHorizSpacing, 0, 12), eyeRot = clampi(m.eyeRotation, 0, 7);
    const int noseType = validIdx(m.noseType, 12), noseSize = clampi(m.noseSize, 0, 8);
    const int noseV = clampi(m.noseVertPos, 0, 18);
    const int lipType = validIdx(m.lipType, 24), lipColor = validIdx(m.lipColor, 3);
    const int lipSize = clampi(m.lipSize, 0, 8), lipV = clampi(m.lipVertPos, 0, 18);
    const int glassesType = validIdx(m.glassesType, 9), glassesColor = validIdx(m.glassesColor, 6);
    const int glassesSize = clampi(m.glassesSize, 0, 7), glassesV = clampi(m.glassesVertPos, 0, 20);
    const int mustacheType = clampi(m.mustacheType, 0, 3), beardType = clampi(m.beardType, 0, 3);
    const int facialHairColor = validIdx(m.facialHairColor, 8);
    const int mustacheSize = clampi(m.mustacheSize, 0, 8), mustacheV = clampi(m.mustacheVertPos, 0, 16);
    const int mole = clampi(m.mole, 0, 1), moleSize = clampi(m.moleSize, 0, 8);
    const int moleV = clampi(m.moleVertPos, 0, 30), moleH = clampi(m.moleHorizPos, 0, 16);

    const f32 flipX = (f32)(-(2 * hairPart - 1));     // +1 normal part, -1 reversed

    GRRLIB_CompoStart();
    GRRLIB_FillScreen(0x00000000);

    // 1. Back hair
    GRRLIB_SetHandle(s_hairs1, 60, 0);
    GRRLIB_SetHandle(s_hairs2, 60, 0);
    GRRLIB_DrawTile(230, 210, s_hairs2, 0, flipX, 1, kHairCol[hairColor], tileOf(kHairBg[kHairFg[hairType]]));

    // 2. Head
    GRRLIB_DrawTile(230, 200, s_heads, 0, 1, 1, kSkinCol[skinColor], tileOf(faceShape));

    // 3. Facial feature
    int feature = 39;
    switch (facialFeature) {
        case 0:  feature = 39; break;
        case 1: case 2: feature = faceShape; break;
        case 3:  feature = 33; break;
        case 4:  feature = 34; break;
        case 5:  feature = 35; break;
        case 6:  feature = 36; break;
        case 7:  feature = 40 + faceShape; break;
        case 8:  feature = 38; break;
        case 9:  feature = 8 + faceShape;  break;
        case 10: feature = 16 + faceShape; break;
        case 11: feature = 24 + faceShape; break;
    }
    if (facialFeature == 2)
        GRRLIB_DrawTile(230, 200, s_features, 0, 1, 1, kSkinCol[skinColor], 32);
    GRRLIB_DrawTile(230, 200, s_features, 0, 1, 1, kSkinCol[skinColor], tileOf(feature));

    // 4. Beard
    if (beardType > 0)
        GRRLIB_DrawTile(230, 200, s_beards, 0, 1, 1, kHairCol[facialHairColor], ((beardType - 1) * 8) + faceShape);

    // 5. Mole
    if (mole == 1) {
        GRRLIB_SetHandle(s_mole, 6, 6);
        const f32 sc = .2f + (moleSize * 0.1f);
        GRRLIB_DrawImg(252 + (4 * moleH), 212 + (2.8f * moleV), s_mole, 0, sc, sc, 0xFFFFFFFF);
    }

    // 6. Eyes (closed tile while blinking, expression override if set)
    {
        int tile = closed_ ? 46 : ((eyes_ == 0) ? kEyes[eyeType] : (eyes_ - 1));
        tile = tileOf(tile);
        const f32 sc  = .3f + (eyeSize * 0.1f);
        const f32 rot = 11.25f * (7 - eyeRot);
        const f32 y   = 184 + (2.8f * eyeV);
        drawEye(272 + (2.6f * eyeH), y, 18,  rot, sc, tile,                 kEyeCol[eyeColor]);
        drawEye(254 - (2.6f * eyeH), y, 36, -rot, sc, tileOf(95 - tile),    kEyeCol[eyeColor]);
    }

    // 7. Eyebrows
    {
        const f32 sc  = .3f + (ebSize * 0.1f);
        const f32 rot = 11.25f * (11 - ebRot);
        const f32 y   = 166 + (2.8f * (ebV - 3));
        const int tile = kEyebrows[ebType];
        GRRLIB_SetHandle(s_eyebrows, 0, 54);
        GRRLIB_DrawTile(288 + (2.6f * ebH), y, s_eyebrows,  rot, sc, sc, kHairCol[ebColor], tileOf(tile));
        GRRLIB_SetHandle(s_eyebrows, 36, 54);
        GRRLIB_DrawTile(256 - (2.6f * ebH), y, s_eyebrows, -rot, sc, sc, kHairCol[ebColor], tileOf(53 - tile));
    }

    // 8. Lips
    {
        const int tile = tileOf((mouth_ == 0) ? kLips[lipType] : (mouth_ - 1));
        const int baseTile = (mouth_ == 0) ? kLips[lipType] : tile;
        u32 col = 0xFFFFFFFF;
        if (baseTile == 1 || baseTile == 6 || baseTile == 11 || baseTile == 17 || baseTile == 19)
            col = (mouth_ == 0) ? kLipCol[lipColor] : 0xC76C46FF;
        const f32 sc = .2f + (lipSize * 0.1f);
        GRRLIB_SetHandle(s_lips, 30, 30);
        GRRLIB_DrawTile(260, 220 + (2.6f * lipV), s_lips, 0, sc, sc, col, tile);
    }

    // 9. Mustache
    if (mustacheType > 0) {
        const f32 sc = .2f + (mustacheSize * 0.1f);
        GRRLIB_SetHandle(s_mustache, 30, 10);
        GRRLIB_DrawTile(260, 244 + (2.9f * mustacheV), s_mustache, 0, sc, sc,
                        kHairCol[facialHairColor], tileOf(mustacheType - 1));
    }

    // 10. Nose
    {
        const f32 sc = .2f + (noseSize * 0.1f);
        GRRLIB_SetHandle(s_noses, 25, 30);
        GRRLIB_DrawTile(265, 220 + (2.6f * noseV), s_noses, 0, sc, sc, kSkinCol[skinColor], tileOf(kNoses[noseType]));
    }

    // 11. Front hair
    {
        const int fg = kHairFg[hairType];
        if (fg < 56) GRRLIB_DrawTile(230, 190, s_hairs1, 0, flipX, 1, kHairCol[hairColor], tileOf(fg));
        else         GRRLIB_DrawTile(230, 190, s_hairs2, 0, flipX, 1, kHairCol[hairColor], tileOf(fg - 56));
    }

    // 12. Glasses
    if (glassesType > 0) {
        const f32 sc = .1f + (glassesSize * 0.1f);
        const f32 y  = 195 + (2.6f * glassesV);
        GRRLIB_SetHandle(s_glasses, 90, 32);
        if (glassesType < 6) {
            GRRLIB_DrawTile(200, y, s_glasses, 0, sc, sc, kGlassesCol[glassesColor], tileOf(glassesType - 1));
        } else {
            GRRLIB_DrawTile(200, y, s_glasses, 0, sc, sc, kGlassesCol[glassesColor], tileOf(glassesType + 2));
            GRRLIB_DrawTile(200, y, s_glasses, 0, sc, sc, 0xFFFFFFFF, tileOf(glassesType - 1));
        }
    }

    GRRLIB_CompoEnd(200, 160, tex_);
}