#pragma once
#include <grrlib.h>

// Standalone Mii struct matching libmii's fields used by miirender.cpp
struct Mii {
    int faceShape;
    int skinColor;
    int facialFeature;

    int hairType;
    int hairColor;
    int hairPart;

    int eyebrowType;
    int eyebrowRotation;
    int eyebrowColor;
    int eyebrowSize;
    int eyebrowVertPos;
    int eyebrowHorizSpacing;

    int eyeType;
    int eyeRotation;
    int eyeVertPos;
    int eyeColor;
    int eyeSize;
    int eyeHorizSpacing;

    int noseType;
    int noseSize;
    int noseVertPos;

    int lipType;
    int lipColor;
    int lipSize;
    int lipVertPos;

    int glassesType;
    int glassesColor;
    int glassesSize;
    int glassesVertPos;

    int mustacheType;
    int beardType;
    int facialHairColor;
    int mustacheSize;
    int mustacheVertPos;

    int mole;
    int moleSize;
    int moleVertPos;
    int moleHorizPos;
};

enum MiiExpression { MII_NEUTRAL, MII_HAPPY, MII_SAD, MII_SHOCKED, MII_MAD };

// Call once after GRRLIB_Init(). Returns false if a sprite sheet failed to load.
bool MiiRender_Init();
void MiiRender_Shutdown();

// Decode one 74-byte Wii Mii record into a Mii struct.
void MiiDecodeRaw(const u8 *rec74, Mii *out);

// Read all Miis from the Wii's NAND (RFL_DB.dat). Returns how many were found (<= maxOut).
int MiiLoadFromNand(Mii *out, int maxOut);

class MiiAvatar {
public:
    MiiAvatar();
    ~MiiAvatar();

    void setMii(const Mii &mii);                 // copies the struct
    void setExpression(MiiExpression e);         // picks a random matching mouth
    void setBlinking(bool on) { blinking_ = on; }

    // Call at the START of a frame, before anything else is drawn
    // (building uses the EFB and clears it afterwards).
    void update();

    // Draw centred on (cx, cy) in 640x480 screen space.
    void draw(f32 cx, f32 cy, f32 scale = 1.0f) const;

private:
    MiiAvatar(const MiiAvatar &);                // non-copyable (owns a texture)
    MiiAvatar &operator=(const MiiAvatar &);
    void rebuild();

    Mii  mii_;
    GRRLIB_texImg *tex_;
    int  eyes_, mouth_;                          // 0 = use the Mii's own, else tile+1
    bool dirty_, blinking_, closed_;
    int  blinkTimer_, blinkRand_, blinkCount_;
    bool blinkActive_;
};