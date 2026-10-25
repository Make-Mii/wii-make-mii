#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <malloc.h>
#include <string> 

#include <grrlib.h>

extern "C" {
    #include <gccore.h>
    #include <wiiuse/wpad.h>
    #include <network.h>
    #include <ogc/isfs.h>
}

#include "miirender.h"
#include "font_ttf.h"
#include "logo_png.h"

#define LISTEN_PORT 8080
#define RFL_DB_SIZE 0x1F1E0

//---------------------------------------------------------------------------------
// 74-Byte Big-Endian Mii Structure
//---------------------------------------------------------------------------------
#pragma pack(push, 1)
struct RawMiiData {
    u16 invalid : 1, isGirl : 1, month : 4, day : 5, favColor : 4, isFavorite : 1;
    u16 name[10];
    u8  height, weight;
    u8  miiID[4], systemID[4];
    u16 faceShape : 3, skinColor : 3, facialFeature : 4, unk1 : 3, mingleOff : 1, unk2 : 1, downloaded : 1;
    u16 hairType : 7, hairColor : 3, hairPart : 1, unk3 : 5;
    u32 eyebrowType : 5, unk4 : 1, eyebrowRot : 4, unk5 : 6, eyebrowColor : 3, eyebrowSize : 4, eyebrowVert : 5, eyebrowHoriz : 4;
    u32 eyeType : 6, unk6 : 2, eyeRot : 3, eyeVert : 5, eyeColor : 3, unk7 : 1, eyeSize : 3, eyeHoriz : 4, unk8 : 5;
    u16 noseType : 4, noseSize : 4, noseVert : 5, unk9 : 3;
    u16 lipType : 5, lipColor : 2, lipSize : 4, lipVert : 5;
    u16 glassesType : 4, glassesColor : 3, unk10 : 1, glassesSize : 3, glassesVert : 5;
    u16 mustacheType : 2, beardType : 2, facialHairColor : 3, mustacheSize : 4, mustacheVert : 5;
    u16 moleOn : 1, moleSize : 4, moleVert : 5, moleHoriz : 5, unk11 : 1;
    u16 creatorName[10];
};
#pragma pack(pop)

//---------------------------------------------------------------------------------
// Global State & Wii Light Aesthetic Colors (0xRRGGBBAA format for GRRLIB)
//---------------------------------------------------------------------------------
static GRRLIB_ttfFont *g_font    = nullptr;
static GRRLIB_texImg  *g_logoTex = nullptr;
static MiiAvatar      *g_avatar  = nullptr;

RawMiiData g_receivedMii = {};
bool g_hasMii = false;
char g_statusMsg[64] = "Waiting for .mii packet...";
alignas(32) static u8 g_db[RFL_DB_SIZE];

// Wii System Menu / Mii Channel Light Palette
constexpr u32 COL_BG_LIGHT    = 0xE6EBF0FF; // Soft Wii light grey-blue background
constexpr u32 COL_PANEL       = 0xF9FAFCFF; // Crisp off-white panel fill
constexpr u32 COL_BORDER      = 0xB8C4D4FF; // Subtle cool grey border
constexpr u32 COL_SHADOW      = 0xD0D8E2FF; // Soft drop-shadow under panels
constexpr u32 COL_TEXT_DARK   = 0x2C3540FF; // Deep charcoal for primary text
constexpr u32 COL_TEXT_LABEL  = 0x5A6878FF; // Slate grey for secondary labels
constexpr u32 COL_WII_BLUE    = 0x0096DCFF; // Signature Wii cyan-blue accent
constexpr u32 COL_GREEN       = 0x28B463FF; // Online / Ready green
constexpr u32 COL_RED         = 0xE04848FF; // Offline / Error red

//---------------------------------------------------------------------------------
// GRRLIB Rounded Box & Text Helpers
//---------------------------------------------------------------------------------
void drawFilledRoundedRect(f32 x, f32 y, f32 w, f32 h, f32 r, u32 color) {
    if (r <= 0.0f) {
        GRRLIB_Rectangle(x, y, w, h, color, true);
        return;
    }
    GRRLIB_Rectangle(x + r, y,     w - 2.0f * r, h,            color, true);
    GRRLIB_Rectangle(x,     y + r, r,            h - 2.0f * r, color, true);
    GRRLIB_Rectangle(x + w - r, y + r, r,        h - 2.0f * r, color, true);

    GRRLIB_Circle(x + r,     y + r,     r, color, true);
    GRRLIB_Circle(x + w - r, y + r,     r, color, true);
    GRRLIB_Circle(x + r,     y + h - r, r, color, true);
    GRRLIB_Circle(x + w - r, y + h - r, r, color, true);
}

void drawRoundedPanel(f32 x, f32 y, f32 w, f32 h, f32 radius, u32 fill, u32 border) {
    drawFilledRoundedRect(x + 2.0f, y + 3.0f, w, h, radius, COL_SHADOW);
    drawFilledRoundedRect(x, y, w, h, radius, border);
    drawFilledRoundedRect(x + 2.0f, y + 2.0f, w - 4.0f, h - 4.0f, radius - 2.0f, fill);
}

void drawTextTTF(int x, int y, u32 size, u32 color, const char *fmt, ...) {
    if (!g_font) return;
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    GRRLIB_PrintfTTF(x, y, g_font, buf, size, color);
}

int getMiiCount() {
    bool weInitialized = (ISFS_Initialize() == ISFS_OK);

    s32 fd = ISFS_Open("/shared2/menu/FaceLib/RFL_DB.dat", ISFS_OPEN_READ);
    if (fd < 0) {
        if (weInitialized) ISFS_Deinitialize();
        return -1; // Failed to open file (missing AHBPROT or file error)
    }

    // Read the database header + 100 slots (7404 bytes minimum)
    alignas(32) static u8 tempDb[RFL_DB_SIZE];
    s32 bytesRead = ISFS_Read(fd, tempDb, RFL_DB_SIZE);
    ISFS_Close(fd);
    if (weInitialized) ISFS_Deinitialize();

    if (bytesRead < 4 + (100 * 74)) {
        return -1;
    }

    const RawMiiData *slots = reinterpret_cast<const RawMiiData*>(tempDb + 4);
    int count = 0;

    for (int i = 0; i < 100; i++) {
        // A slot is populated if the Mii ID is non-zero
        if (slots[i].miiID[0] != 0 || slots[i].miiID[1] != 0 ||
            slots[i].miiID[2] != 0 || slots[i].miiID[3] != 0) {
            count++;
        }
    }

    return count;
}

//---------------------------------------------------------------------------------
// NAND Mii Plaza Save Logic
//---------------------------------------------------------------------------------
bool saveToMiiPlaza() {
    int mii_count = getMiiCount();
    std::string miiNameStr = "Mii_" + std::to_string(mii_count);
    const char *newName = miiNameStr.c_str(); 
    memset(g_receivedMii.name, 0, sizeof(g_receivedMii.name));
    for (int i = 0; newName[i] != '\0'; i++) g_receivedMii.name[i] = newName[i];

    //Generate a guaranteed unique 4-byte miiID for every save
    g_receivedMii.invalid = 0;
    u32 uniqueVal = (static_cast<u32>(rand()) << 8) ^ static_cast<u32>(mii_count);
    g_receivedMii.miiID[0] = 0x80 | ((uniqueVal >> 24) & 0x1F);
    g_receivedMii.miiID[1] = (uniqueVal >> 16) & 0xFF;
    g_receivedMii.miiID[2] = (uniqueVal >> 8)  & 0xFF;
    g_receivedMii.miiID[3] = (uniqueVal & 0xFE) | 1; // Ensure non-zero

    // Try initializing, but ignore error if already active (ISFS_ERROR -102 / ISFS_OK 0)
    s32 isfs_ret = ISFS_Initialize();
    bool weInitialized = (isfs_ret == ISFS_OK);

    s32 fd = ISFS_Open("/shared2/menu/FaceLib/RFL_DB.dat", ISFS_OPEN_RW);
    if (fd < 0) {
        if (weInitialized) ISFS_Deinitialize();
        return false;
    }

    bool ok = false;
    if (ISFS_Read(fd, g_db, RFL_DB_SIZE) == RFL_DB_SIZE) {
        RawMiiData *slots = reinterpret_cast<RawMiiData*>(g_db + 4);
        if (slots[0].systemID[0] != 0 || slots[0].systemID[1] != 0) {
            memcpy(g_receivedMii.systemID, slots[0].systemID, 4);
        }

        int target = -1;
        for (int i = 0; i < 100; i++) {
            if (memcmp(slots[i].miiID, g_receivedMii.miiID, 4) == 0) { target = i; break; }
            if (target == -1 && slots[i].miiID[0] == 0 && slots[i].miiID[1] == 0) target = i;
        }
        if (target == -1) target = 0;
        slots[target] = g_receivedMii;

        u32 crc = 0;
        for (u32 i = 0; i < RFL_DB_SIZE - 2; i++) {
            for (int b = 7; b >= 0; b--)
                crc = ((crc << 1) | ((g_db[i] >> b) & 1)) ^ ((crc & 0x8000) ? 0x1021 : 0);
        }
        for (int b = 0; b < 16; b++) crc = (crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0);
        *reinterpret_cast<u16*>(g_db + RFL_DB_SIZE - 2) = static_cast<u16>(crc);

        ISFS_Seek(fd, 0, SEEK_SET);
        ok = (ISFS_Write(fd, g_db, RFL_DB_SIZE) == RFL_DB_SIZE);
    }

    ISFS_Close(fd);
    // Keep ISFS open for the life of the application or only deinit if we started it
    return ok;
}

//---------------------------------------------------------------------------------
// Main Layout Renderer
//---------------------------------------------------------------------------------
void renderAppUI(bool netActive, const char *localIP) {
    // Clear background to soft Wii light grey
    GRRLIB_FillScreen(COL_BG_LIGHT);

    // 1. TOP STATUS BAR
    drawRoundedPanel(20, 16, 600, 38, 10.0f, COL_PANEL, COL_BORDER);
    u32 ledColor = netActive ? COL_GREEN : COL_RED;
    GRRLIB_Circle(42, 35, 8, ledColor, true);
    drawTextTTF(60, 23, 18, COL_TEXT_DARK, "NET: %s   |   WII IP: %s:%d",
                netActive ? "ONLINE" : "OFFLINE", localIP, LISTEN_PORT);

    // 2. LEFT-HAND ATTRIBUTE LIST PANEL (x=20, w=254)
    drawRoundedPanel(20, 66, 254, 310, 14.0f, COL_PANEL, COL_BORDER);
    drawTextTTF(36, 76, 16, COL_WII_BLUE, "MII ATTRIBUTES");
    GRRLIB_Rectangle(36, 100, 222, 2, COL_BORDER, true);

    int attrY = 108;
    int lineStep = 20;
    const RawMiiData &m = g_receivedMii;

    drawTextTTF(36, attrY, 14, COL_TEXT_DARK,  "Gender    : %u", m.isGirl);                       attrY += lineStep;
    drawTextTTF(36, attrY, 14, COL_TEXT_DARK,  "Fav Color : %u", m.favColor);                     attrY += lineStep;
    drawTextTTF(36, attrY, 14, COL_TEXT_DARK,  "Height/Wt : %u / %u", m.height, m.weight);        attrY += lineStep;
    drawTextTTF(36, attrY, 14, COL_TEXT_LABEL, "Face/Skin : %u / %u", m.faceShape, m.skinColor);  attrY += lineStep;
    drawTextTTF(36, attrY, 14, COL_TEXT_LABEL, "Feature   : %u", m.facialFeature);                attrY += lineStep;
    drawTextTTF(36, attrY, 14, COL_TEXT_LABEL, "Hair/Col  : %u / %u", m.hairType, m.hairColor);   attrY += lineStep;
    drawTextTTF(36, attrY, 14, COL_TEXT_LABEL, "Brows/Col : %u / %u", m.eyebrowType, m.eyebrowColor); attrY += lineStep;
    drawTextTTF(36, attrY, 14, COL_TEXT_LABEL, "Eyes/Col  : %u / %u", m.eyeType, m.eyeColor);     attrY += lineStep;
    drawTextTTF(36, attrY, 14, COL_TEXT_LABEL, "Nose/Size : %u / %u", m.noseType, m.noseSize);    attrY += lineStep;
    drawTextTTF(36, attrY, 14, COL_TEXT_LABEL, "Mouth/Col : %u / %u", m.lipType, m.lipColor);     attrY += lineStep;
    drawTextTTF(36, attrY, 14, COL_TEXT_DARK,  "Glasses   : %u", m.glassesType);                  attrY += lineStep;
    drawTextTTF(36, attrY, 14, COL_TEXT_DARK,  "Beard/Mst : %u / %u", m.beardType, m.mustacheType); attrY += lineStep;
    drawTextTTF(36, attrY, 14, COL_TEXT_DARK,  "Mole      : %u", m.moleOn);

    // 3. TOP-RIGHT FACE PREVIEW PANEL (x=290, y=66, w=330, h=310)
    drawRoundedPanel(290, 66, 330, 310, 14.0f, COL_PANEL, COL_BORDER);
    drawTextTTF(306, 76, 16, COL_TEXT_LABEL, "MII PREVIEW");
    GRRLIB_Rectangle(306, 100, 298, 2, COL_BORDER, true);

    if (g_hasMii && g_avatar) {
        g_avatar->draw(430.0f, 205.0f, 1.3f);
    } else {
        // Show waiting text centered in the preview box
        drawTextTTF(400, 230, 16, COL_RED, "Waiting for Mii...");
    }

    // 4. BOTTOM-LEFT LOGO SPACE
    drawRoundedPanel(20, 392, 160, 80, 12.0f, COL_PANEL, COL_BORDER);
    if (g_logoTex) {
        GRRLIB_DrawImg(38, 390, g_logoTex, 0, 0.5f, 0.5f, 0xFFFFFFFF);
    } else {
        drawTextTTF(52, 415, 16, COL_TEXT_LABEL, "LOGO HERE");
    }

    // 5. BOTTOM INSTRUCTION BAR
    drawRoundedPanel(196, 392, 424, 80, 12.0f, COL_PANEL, COL_BORDER);
    drawTextTTF(214, 410, 18, g_hasMii ? COL_GREEN : COL_TEXT_DARK,
                "Press [A] to save Mii to Plaza");
    drawTextTTF(214, 436, 15, COL_WII_BLUE, "%s", g_statusMsg);
}

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------
    // 1. Initialize Network BEFORE GRRLIB_Init so DHCP wait doesn't stall GX FIFO
    net_init();

    char localIP[16] = "0.0.0.0";
    bool netActive = (if_config(localIP, nullptr, nullptr, true, 20) >= 0);
    s32 sock = -1;
    if (netActive) {
        sock = net_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock >= 0) {
            u32 nonBlocking = 1;
            net_ioctl(sock, FIONBIO, &nonBlocking);

            struct sockaddr_in addr = {};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(LISTEN_PORT);
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
            if (net_bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
                netActive = false;
            }
        } else {
            netActive = false;
        }
    }

    // 2. Initialize GRRLIB, Wiimote, and Mii Renderer Sprite Sheets
    GRRLIB_Init();
    WPAD_Init();
    bool miiRenderReady = MiiRender_Init();

    // 3. Load TrueType Font, PNG Logo, and create MiiAvatar instance
    g_font    = GRRLIB_LoadTTF(font_ttf, font_ttf_size);
    g_logoTex = GRRLIB_LoadTexturePNG(logo_png);

    if (miiRenderReady) {
        g_avatar = new MiiAvatar();
    }

    // 4. Main 60 FPS Loop
    while (SYS_MainLoop()) {
        WPAD_ScanPads();
        u32 pressed = WPAD_ButtonsDown(0);
        if (pressed & WPAD_BUTTON_HOME) break;

        // Poll UDP for incoming 74-byte .mii packets
        if (netActive && sock >= 0) {
            RawMiiData incoming;
            struct sockaddr_in client;
            u32 clientLen = sizeof(client);
            if (net_recvfrom(sock, &incoming, sizeof(RawMiiData), 0,
                             reinterpret_cast<struct sockaddr*>(&client), &clientLen) == sizeof(RawMiiData)) {
                g_receivedMii = incoming;
                g_hasMii = true;

                // Decode raw 74-byte packet into Mii struct and update avatar preview
                if (g_avatar) {
                    Mii decodedMii;
                    MiiDecodeRaw(reinterpret_cast<const u8*>(&g_receivedMii), &decodedMii);
                    g_avatar->setMii(decodedMii);
                }
                snprintf(g_statusMsg, sizeof(g_statusMsg), "Received Mii! Ready to save.");
            }
        }

        // Optional: Expressions on D-Pad / 1 / 2 buttons
        if (g_avatar) {
            if (pressed & WPAD_BUTTON_UP)   g_avatar->setExpression(MII_HAPPY);
            if (pressed & WPAD_BUTTON_DOWN) g_avatar->setExpression(MII_NEUTRAL);
            if (pressed & WPAD_BUTTON_B)    g_avatar->setExpression(MII_SAD);
            if (pressed & WPAD_BUTTON_1)    g_avatar->setExpression(MII_SHOCKED);
            if (pressed & WPAD_BUTTON_2)    g_avatar->setExpression(MII_MAD);
        }

        // Save to NAND Mii Plaza on [A]
        if ((pressed & WPAD_BUTTON_A) && g_hasMii) {
            if (saveToMiiPlaza()) {
                g_hasMii = false;
                if (g_avatar) g_avatar->setExpression(MII_HAPPY);
                snprintf(g_statusMsg, sizeof(g_statusMsg), "Saved 'NewMii' to Mii Plaza!");
            } else {
                snprintf(g_statusMsg, sizeof(g_statusMsg), "Error: Could not write RFL_DB.dat");
            }
        }

        // IMPORTANT: Call avatar->update() FIRST before drawing the UI!
        // (rebuild() uses EFB composition and clears the screen when dirty/blinking)
        if (g_avatar) {
            g_avatar->update();
        }

        // Draw UI and push frame to screen
        renderAppUI(netActive, localIP);
        GRRLIB_Render();
    }

    // Clean up resources on exit
    if (g_avatar) {
        delete g_avatar;
        g_avatar = nullptr;
    }
    MiiRender_Shutdown();
    GRRLIB_FreeTTF(g_font);
    GRRLIB_FreeTexture(g_logoTex);
    GRRLIB_Exit();

    return 0;
}