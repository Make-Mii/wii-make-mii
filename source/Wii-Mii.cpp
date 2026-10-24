#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <malloc.h>

#include <grrlib.h>

extern "C" {
    #include <gccore.h>
    #include <wiiuse/wpad.h>
    #include <network.h>
    #include <ogc/isfs.h>
}

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
static GRRLIB_ttfFont *g_font = nullptr;
static GRRLIB_texImg  *g_logoTex = nullptr;

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

// Draws a filled rounded rectangle using overlapping rects and 4 corner circles
void drawFilledRoundedRect(f32 x, f32 y, f32 w, f32 h, f32 r, u32 color) {
    if (r <= 0.0f) {
        GRRLIB_Rectangle(x, y, w, h, color, true);
        return;
    }
    // Center & vertical/horizontal spans
    GRRLIB_Rectangle(x + r, y,     w - 2.0f * r, h,            color, true);
    GRRLIB_Rectangle(x,     y + r, r,            h - 2.0f * r, color, true);
    GRRLIB_Rectangle(x + w - r, y + r, r,        h - 2.0f * r, color, true);

    // Four rounded corners
    GRRLIB_Circle(x + r,     y + r,     r, color, true);
    GRRLIB_Circle(x + w - r, y + r,     r, color, true);
    GRRLIB_Circle(x + r,     y + h - r, r, color, true);
    GRRLIB_Circle(x + w - r, y + h - r, r, color, true);
}

// Draws a Wii-style rounded panel with a subtle drop shadow and 2px border
void drawRoundedPanel(f32 x, f32 y, f32 w, f32 h, f32 radius, u32 fill, u32 border) {
    // Subtle 2px bottom drop-shadow for depth
    drawFilledRoundedRect(x + 2.0f, y + 3.0f, w, h, radius, COL_SHADOW);
    // Outer border shape
    drawFilledRoundedRect(x, y, w, h, radius, border);
    // Inner panel fill (inset by 2px)
    drawFilledRoundedRect(x + 2.0f, y + 2.0f, w - 4.0f, h - 4.0f, radius - 2.0f, fill);
}

// Formatted printf wrapper around GRRLIB_PrintfTTF
void drawTextTTF(int x, int y, u32 size, u32 color, const char *fmt, ...) {
    if (!g_font) return;
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    GRRLIB_PrintfTTF(x, y, g_font, buf, size, color);
}

//---------------------------------------------------------------------------------
// NAND Mii Plaza Save Logic (Unchanged)
//---------------------------------------------------------------------------------
bool saveToMiiPlaza() {
    const char *newName = "NewMii";
    memset(g_receivedMii.name, 0, sizeof(g_receivedMii.name));
    for (int i = 0; newName[i] != '\0'; i++) g_receivedMii.name[i] = newName[i];

    g_receivedMii.invalid = 0;
    g_receivedMii.miiID[0] = (g_receivedMii.miiID[0] & 0x1F) | 0x80;
    if (g_receivedMii.miiID[1] == 0 && g_receivedMii.miiID[2] == 0 && g_receivedMii.miiID[3] == 0) {
        g_receivedMii.miiID[3] = 1;
    }

    if (ISFS_Initialize() != ISFS_OK) return false;
    s32 fd = ISFS_Open("/shared2/menu/FaceLib/RFL_DB.dat", ISFS_OPEN_RW);
    if (fd < 0) {
        ISFS_Deinitialize();
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
    ISFS_Deinitialize();
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
    GRRLIB_Circle(42, 35, 8, ledColor, true); // Smooth circular status LED
    drawTextTTF(60, 23, 18, COL_TEXT_DARK, "NET: %s   |   WII IP: %s:%d",
                netActive ? "ONLINE" : "OFFLINE", localIP, LISTEN_PORT);

    // 2. FACE PREVIEW AREA (Left/Top-Left viewport reserved for your Mii renderer)
    drawRoundedPanel(20, 66, 330, 310, 14.0f, COL_PANEL, COL_BORDER);
    drawTextTTF(36, 76, 16, COL_TEXT_LABEL, "MII PREVIEW");
    GRRLIB_Rectangle(36, 100, 298, 2, COL_BORDER, true);
    // --> Your Mii Face Rendering Call Goes Here <--

    // 3. RIGHT-HAND ATTRIBUTE LIST PANEL
    drawRoundedPanel(366, 66, 254, 310, 14.0f, COL_PANEL, COL_BORDER);
    drawTextTTF(382, 76, 16, COL_WII_BLUE, "MII ATTRIBUTES");
    GRRLIB_Rectangle(382, 100, 222, 2, COL_BORDER, true);

    int attrY = 108;
    int lineStep = 20;
    const RawMiiData &m = g_receivedMii;

    drawTextTTF(382, attrY, 14, COL_TEXT_DARK,  "Gender    : %u", m.isGirl);                       attrY += lineStep;
    drawTextTTF(382, attrY, 14, COL_TEXT_DARK,  "Fav Color : %u", m.favColor);                     attrY += lineStep;
    drawTextTTF(382, attrY, 14, COL_TEXT_DARK,  "Height/Wt : %u / %u", m.height, m.weight);        attrY += lineStep;
    drawTextTTF(382, attrY, 14, COL_TEXT_LABEL, "Face/Skin : %u / %u", m.faceShape, m.skinColor);  attrY += lineStep;
    drawTextTTF(382, attrY, 14, COL_TEXT_LABEL, "Feature   : %u", m.facialFeature);                attrY += lineStep;
    drawTextTTF(382, attrY, 14, COL_TEXT_LABEL, "Hair/Col  : %u / %u", m.hairType, m.hairColor);   attrY += lineStep;
    drawTextTTF(382, attrY, 14, COL_TEXT_LABEL, "Brows/Col : %u / %u", m.eyebrowType, m.eyebrowColor); attrY += lineStep;
    drawTextTTF(382, attrY, 14, COL_TEXT_LABEL, "Eyes/Col  : %u / %u", m.eyeType, m.eyeColor);     attrY += lineStep;
    drawTextTTF(382, attrY, 14, COL_TEXT_LABEL, "Nose/Size : %u / %u", m.noseType, m.noseSize);    attrY += lineStep;
    drawTextTTF(382, attrY, 14, COL_TEXT_LABEL, "Mouth/Col : %u / %u", m.lipType, m.lipColor);     attrY += lineStep;
    drawTextTTF(382, attrY, 14, COL_TEXT_DARK,  "Glasses   : %u", m.glassesType);                  attrY += lineStep;
    drawTextTTF(382, attrY, 14, COL_TEXT_DARK,  "Beard/Mst : %u / %u", m.beardType, m.mustacheType); attrY += lineStep;
    drawTextTTF(382, attrY, 14, COL_TEXT_DARK,  "Mole      : %u", m.moleOn);

    // 4. BOTTOM-LEFT LOGO SPACE
    drawRoundedPanel(20, 392, 160, 64, 12.0f, COL_PANEL, COL_BORDER);
    if (g_logoTex) {
        GRRLIB_DrawImg(24, 396, g_logoTex, 0, 1.0f, 1.0f, 0xFFFFFFFF);
    } else {
        drawTextTTF(52, 415, 16, COL_TEXT_LABEL, "LOGO HERE");
    }

    // 5. BOTTOM INSTRUCTION BAR
    drawRoundedPanel(196, 392, 424, 64, 12.0f, COL_PANEL, COL_BORDER);
    drawTextTTF(214, 402, 18, g_hasMii ? COL_GREEN : COL_TEXT_DARK,
                "Press [A] to save Mii to Plaza");
    drawTextTTF(214, 428, 15, COL_WII_BLUE, "%s", g_statusMsg);
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

    // 2. Initialize GRRLIB & Wiimote
    GRRLIB_Init();
    WPAD_Init();

    // 3. Load TrueType Font and PNG Logo from memory buffers
    g_font    = GRRLIB_LoadTTF(font_ttf, font_ttf_size);
    g_logoTex = GRRLIB_LoadTexturePNG(logo_png);

    // 4. Main 60 FPS Loop
    while (SYS_MainLoop()) {
        WPAD_ScanPads();
        u32 pressed = WPAD_ButtonsDown(0);
        if (pressed & WPAD_BUTTON_HOME) break;

        if (netActive && sock >= 0) {
            RawMiiData incoming;
            struct sockaddr_in client;
            u32 clientLen = sizeof(client);
            if (net_recvfrom(sock, &incoming, sizeof(RawMiiData), 0,
                             reinterpret_cast<struct sockaddr*>(&client), &clientLen) == sizeof(RawMiiData)) {
                g_receivedMii = incoming;
                g_hasMii = true;
                snprintf(g_statusMsg, sizeof(g_statusMsg), "Received Mii! Ready to save.");
            }
        }

        if ((pressed & WPAD_BUTTON_A) && g_hasMii) {
            if (saveToMiiPlaza()) {
                g_hasMii = false;
                snprintf(g_statusMsg, sizeof(g_statusMsg), "Saved 'NewMii' to Mii Plaza!");
            } else {
                snprintf(g_statusMsg, sizeof(g_statusMsg), "Error: Could not write RFL_DB.dat");
            }
        }

        // Draw UI and push frame to screen
        renderAppUI(netActive, localIP);
        GRRLIB_Render();
    }

    // Clean up resources on exit
    GRRLIB_FreeTTF(g_font);
    GRRLIB_FreeTexture(g_logoTex);
    GRRLIB_Exit();

    return 0;
}