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

// Custom Mii sprite renderer and bin2o generated asset headers
#include "miirender.h"
#include "font_ttf.h"
#include "logo_png.h"

// Network UDP listening port and Wii NAND Mii database byte size (127,456 bytes)
#define LISTEN_PORT 8080
#define RFL_DB_SIZE 0x1F1E0

//---------------------------------------------------------------------------------
// 74-Byte Big-Endian Mii Structure
// Matches the official Nintendo RFL_DB.dat binary slot layout bit-for-bit
//---------------------------------------------------------------------------------
#pragma pack(push, 1)
struct RawMiiData {
    // Bytes 0x00 - 0x01: Core metadata, birthday, and favorite color index (0-11)
    u16 invalid : 1, isGirl : 1, month : 4, day : 5, favColor : 4, isFavorite : 1;
    // Bytes 0x02 - 0x15: UTF-16BE Mii name (up to 10 characters, zero-padded)
    u16 name[10];
    // Bytes 0x16 - 0x17: Physical body proportions (0-127)
    u8  height, weight;
    // Bytes 0x18 - 0x1F: Unique 4-byte Mii ID and 4-byte originating console MAC/System ID
    u8  miiID[4], systemID[4];
    // Bytes 0x20 - 0x21: Face shape, skin tone, wrinkles/blush, and mingle flags
    u16 faceShape : 3, skinColor : 3, facialFeature : 4, unk1 : 3, mingleOff : 1, unk2 : 1, downloaded : 1;
    // Bytes 0x22 - 0x23: Hairstyle ID, hair color, and flipped part flag
    u16 hairType : 7, hairColor : 3, hairPart : 1, unk3 : 5;
    // Bytes 0x24 - 0x27: Eyebrow style, rotation, color, scale, vertical & horizontal position
    u32 eyebrowType : 5, unk4 : 1, eyebrowRot : 4, unk5 : 6, eyebrowColor : 3, eyebrowSize : 4, eyebrowVert : 5, eyebrowHoriz : 4;
    // Bytes 0x28 - 0x2B: Eye style, rotation, vertical placement, color, scale, and spacing
    u32 eyeType : 6, unk6 : 2, eyeRot : 3, eyeVert : 5, eyeColor : 3, unk7 : 1, eyeSize : 3, eyeHoriz : 4, unk8 : 5;
    // Bytes 0x2C - 0x2D: Nose style, scale, and vertical placement
    u16 noseType : 4, noseSize : 4, noseVert : 5, unk9 : 3;
    // Bytes 0x2E - 0x2F: Mouth/lip style, color, scale, and vertical placement
    u16 lipType : 5, lipColor : 2, lipSize : 4, lipVert : 5;
    // Bytes 0x30 - 0x31: Glasses style, tint color, scale, and vertical placement
    u16 glassesType : 4, glassesColor : 3, unk10 : 1, glassesSize : 3, glassesVert : 5;
    // Bytes 0x32 - 0x33: Mustache and goatee styles, facial hair color, scale, and placement
    u16 mustacheType : 2, beardType : 2, facialHairColor : 3, mustacheSize : 4, mustacheVert : 5;
    // Bytes 0x34 - 0x35: Beauty mark / mole visibility, scale, and X/Y coordinates
    u16 moleOn : 1, moleSize : 4, moleVert : 5, moleHoriz : 5, unk11 : 1;
    // Bytes 0x36 - 0x49: UTF-16BE Creator name (up to 10 characters, zero-padded)
    u16 creatorName[10];
};
#pragma pack(pop)

//---------------------------------------------------------------------------------
// Global State & Wii Light Aesthetic Colors (0xRRGGBBAA format for GRRLIB)
//---------------------------------------------------------------------------------
static GRRLIB_ttfFont *g_font    = nullptr; // Loaded TrueType font handle
static GRRLIB_texImg  *g_logoTex = nullptr; // Loaded PNG logo texture handle
static MiiAvatar      *g_avatar  = nullptr; // Off-screen EFB Mii head composite renderer

RawMiiData g_receivedMii = {}; // Buffer holding the currently active/displayed 74-byte Mii
bool g_hasMii    = false;      // True once at least one valid 74-byte UDP packet has been received
bool g_isPaused  = false;      // false = LIVE_UPDATE, true = PAUSED (inspecting Mii)
int  g_totalMiis = -1;         // Cached count of Miis currently stored in NAND Plaza (-1 = error/unloaded)
u32  g_pktCount  = 0;          // Total number of valid 74-byte Mii UDP packets received this session

char g_statusMsg[64] = "Waiting for live .mii stream..."; // Bottom instruction bar status text
alignas(32) static u8 g_db[RFL_DB_SIZE];                  // 32-byte aligned buffer for NAND DMA reads/writes

// Wii System Menu / Mii Channel Light Palette
constexpr u32 COL_BG_LIGHT    = 0xE6EBF0FF; // Soft Wii light grey-blue background
constexpr u32 COL_PANEL       = 0xF9FAFCFF; // Crisp off-white panel fill
constexpr u32 COL_BORDER      = 0xB8C4D4FF; // Subtle cool grey border
constexpr u32 COL_SHADOW      = 0xD0D8E2FF; // Soft drop-shadow under panels
constexpr u32 COL_TEXT_DARK   = 0x2C3540FF; // Deep charcoal for primary text
constexpr u32 COL_TEXT_LABEL  = 0x5A6878FF; // Slate grey for secondary labels
constexpr u32 COL_WII_BLUE    = 0x0096DCFF; // Signature Wii cyan-blue accent
constexpr u32 COL_GREEN       = 0x28B463FF; // Online / Live green
constexpr u32 COL_AMBER       = 0xF39C12FF; // Paused / Inspecting amber-gold
constexpr u32 COL_RED         = 0xE04848FF; // Offline / Error red

//---------------------------------------------------------------------------------
// GRRLIB Rounded Box & Text Helpers
//---------------------------------------------------------------------------------

// Draws a solid filled rectangle with rounded corners of radius `r`
// Constructed from a center span, two side spans, and four corner circles
void drawFilledRoundedRect(f32 x, f32 y, f32 w, f32 h, f32 r, u32 color) {
    // Fallback to standard sharp rectangle if radius is zero or negative
    if (r <= 0.0f) {
        GRRLIB_Rectangle(x, y, w, h, color, true);
        return;
    }
    // Draw main vertical and horizontal body spans
    GRRLIB_Rectangle(x + r, y,     w - 2.0f * r, h,            color, true);
    GRRLIB_Rectangle(x,     y + r, r,            h - 2.0f * r, color, true);
    GRRLIB_Rectangle(x + w - r, y + r, r,        h - 2.0f * r, color, true);

    // Fill in the four rounded corners
    GRRLIB_Circle(x + r,     y + r,     r, color, true);
    GRRLIB_Circle(x + w - r, y + r,     r, color, true);
    GRRLIB_Circle(x + r,     y + h - r, r, color, true);
    GRRLIB_Circle(x + w - r, y + h - r, r, color, true);
}

// Draws a Wii-style UI panel with an offset drop shadow, 2px outer border, and inner fill
void drawRoundedPanel(f32 x, f32 y, f32 w, f32 h, f32 radius, u32 fill, u32 border) {
    drawFilledRoundedRect(x + 2.0f, y + 3.0f, w, h, radius, COL_SHADOW);
    drawFilledRoundedRect(x, y, w, h, radius, border);
    drawFilledRoundedRect(x + 2.0f, y + 2.0f, w - 4.0f, h - 4.0f, radius - 2.0f, fill);
}

// Printf-style wrapper around GRRLIB_PrintfTTF for formatted string rendering
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
// NAND Mii Plaza Helpers
//---------------------------------------------------------------------------------

// Opens /shared2/menu/FaceLib/RFL_DB.dat in read-only mode and counts occupied Mii slots
int getMiiCount() {
    // Initialize ISFS if not already active in this session
    bool weInitialized = (ISFS_Initialize() == ISFS_OK);

    s32 fd = ISFS_Open("/shared2/menu/FaceLib/RFL_DB.dat", ISFS_OPEN_READ);
    if (fd < 0) {
        if (weInitialized) ISFS_Deinitialize();
        return -1; // Failed to open database (e.g., missing AHBPROT permissions)
    }

    // Read the entire database into a 32-byte aligned buffer
    alignas(32) static u8 tempDb[RFL_DB_SIZE];
    s32 bytesRead = ISFS_Read(fd, tempDb, RFL_DB_SIZE);
    ISFS_Close(fd);
    if (weInitialized) ISFS_Deinitialize();

    // Ensure at least the 4-byte magic header ("RNOD") + 100 74-byte slots were read
    if (bytesRead < 4 + (100 * 74)) {
        return -1;
    }

    // Slot 0 begins immediately after the 4-byte file header
    const RawMiiData *slots = reinterpret_cast<const RawMiiData*>(tempDb + 4);
    int count = 0;

    // A slot is occupied if its 4-byte miiID is non-zero
    for (int i = 0; i < 100; i++) {
        if (slots[i].miiID[0] != 0 || slots[i].miiID[1] != 0 ||
            slots[i].miiID[2] != 0 || slots[i].miiID[3] != 0) {
            count++;
        }
    }

    return count;
}

// Injects `g_receivedMii` into the first empty slot in RFL_DB.dat and updates the file CRC-16
bool saveToMiiPlaza(char *outSavedName, size_t maxNameLen) {
    // Query current Plaza population to assign an incrementing name ("Mii_1", "Mii_2", etc.)
    int mii_count = getMiiCount();
    if (mii_count < 0) mii_count = 0;

    std::string miiNameStr = "Mii_" + std::to_string(mii_count + 1);
    const char *newName = miiNameStr.c_str();
    if (outSavedName && maxNameLen > 0) {
        snprintf(outSavedName, maxNameLen, "%s", newName);
    }

    // Convert ASCII characters into the Mii's big-endian UTF-16 name array
    memset(g_receivedMii.name, 0, sizeof(g_receivedMii.name));
    for (int i = 0; newName[i] != '\0' && i < 10; i++) {
        g_receivedMii.name[i] = newName[i];
    }

    // Generate a guaranteed unique 4-byte miiID for every save so slots don't overwrite each other
    g_receivedMii.invalid = 0;
    u32 uniqueVal = (static_cast<u32>(rand()) << 8) ^ static_cast<u32>(mii_count + 1);
    g_receivedMii.miiID[0] = 0x80 | ((uniqueVal >> 24) & 0x1F); // Top bits 100 = normal local Mii timestamp prefix
    g_receivedMii.miiID[1] = (uniqueVal >> 16) & 0xFF;
    g_receivedMii.miiID[2] = (uniqueVal >> 8)  & 0xFF;
    g_receivedMii.miiID[3] = (uniqueVal & 0xFE) | 1; // Ensure non-zero

    // Open the NAND Mii database with Read/Write access
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

        // Inherit the console's 4-byte System ID from slot 0 if present so the Wii recognizes ownership
        if (slots[0].systemID[0] != 0 || slots[0].systemID[1] != 0) {
            memcpy(g_receivedMii.systemID, slots[0].systemID, 4);
        }

        // Find the first empty slot in the 100-slot Plaza array
        int target = -1;
        for (int i = 0; i < 100; i++) {
            if (slots[i].miiID[0] == 0 && slots[i].miiID[1] == 0 &&
                slots[i].miiID[2] == 0 && slots[i].miiID[3] == 0) {
                target = i;
                break;
            }
        }
        // Fallback to slot 0 if all 100 slots are full
        if (target == -1) target = 0;
        slots[target] = g_receivedMii;

        // Recalculate the CCITT CRC-16 (polynomial 0x1021) over the entire file minus the final 2 CRC bytes
        u32 crc = 0;
        for (u32 i = 0; i < RFL_DB_SIZE - 2; i++) {
            for (int b = 7; b >= 0; b--)
                crc = ((crc << 1) | ((g_db[i] >> b) & 1)) ^ ((crc & 0x8000) ? 0x1021 : 0);
        }
        for (int b = 0; b < 16; b++) crc = (crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0);

        // Write the new 16-bit checksum into the final 2 bytes of the database buffer
        *reinterpret_cast<u16*>(g_db + RFL_DB_SIZE - 2) = static_cast<u16>(crc);

        // Rewind file pointer and commit the updated database back to NAND flash
        ISFS_Seek(fd, 0, SEEK_SET);
        ok = (ISFS_Write(fd, g_db, RFL_DB_SIZE) == RFL_DB_SIZE);
    }

    ISFS_Close(fd);
    if (weInitialized) ISFS_Deinitialize();
    return ok;
}

//---------------------------------------------------------------------------------
// Main Layout Renderer
// Renders the 5-panel Wii dashboard at 640x480 resolution
//---------------------------------------------------------------------------------
void renderAppUI(bool netActive, const char *localIP) {
    // Clear screen background to Wii light grey-blue
    GRRLIB_FillScreen(COL_BG_LIGHT);

    // 1. TOP STATUS BAR (Network status LED, Wii IP:Port, and NAND Plaza capacity)
    drawRoundedPanel(20, 14, 600, 38, 10.0f, COL_PANEL, COL_BORDER);
    u32 ledColor = netActive ? COL_GREEN : COL_RED;
    GRRLIB_Circle(40, 33, 7, ledColor, true);
    drawTextTTF(56, 21, 17, COL_TEXT_DARK, "NET: %s  |  %s:%d",
                netActive ? "ONLINE" : "OFFLINE", localIP, LISTEN_PORT);

    if (g_totalMiis >= 0) {
        drawTextTTF(465, 21, 17, COL_WII_BLUE, "PLAZA: %d/100", g_totalMiis);
    } else {
        drawTextTTF(465, 21, 17, COL_TEXT_LABEL, "PLAZA: --/100");
    }

    // 2. LEFT-HAND ATTRIBUTE LIST PANEL (x=20, y=62, w=254, h=234)
    drawRoundedPanel(20, 62, 254, 256, 14.0f, COL_PANEL, COL_BORDER);
    drawTextTTF(36, 72, 20, COL_WII_BLUE, "MII ATTRIBUTES");
    GRRLIB_Rectangle(36, 104, 222, 2, COL_BORDER, true);

    int attrY = 122;
    int lineStep = 26;
    const RawMiiData &m = g_receivedMii;

    // Display decoded facial feature indices if a Mii has been received
    if (g_hasMii) {
        drawTextTTF(36, attrY, 16, COL_TEXT_DARK, "Face/Skin : %u / %u", m.faceShape, m.skinColor);attrY += lineStep;
        drawTextTTF(36, attrY, 16, COL_TEXT_DARK, "Feature : %u", m.facialFeature);              attrY += lineStep;
        drawTextTTF(36, attrY, 16, COL_TEXT_DARK, "Hair/Col : %u / %u", m.hairType, m.hairColor); attrY += lineStep;
        drawTextTTF(36, attrY, 16, COL_TEXT_DARK, "Brows/Col : %u / %u", m.eyebrowType, m.eyebrowColor); attrY += lineStep;
        drawTextTTF(36, attrY, 16, COL_TEXT_DARK, "Eyes/Col : %u / %u", m.eyeType, m.eyeColor);   attrY += lineStep;
        drawTextTTF(36, attrY, 16, COL_TEXT_DARK, "Nose/Size : %u / %u", m.noseType, m.noseSize);  attrY += lineStep;
        drawTextTTF(36, attrY, 16, COL_TEXT_DARK, "Mouth/Col : %u / %u", m.lipType, m.lipColor);   attrY += lineStep;
    } else {
        drawTextTTF(48, 196, 15, COL_RED, "No Mii data loaded");
    }

    // 3. TOP-RIGHT FACE PREVIEW PANEL (x=290, y=62, w=330, h=318)
    drawRoundedPanel(290, 62, 330, 318, 14.0f, COL_PANEL, COL_BORDER);
    drawTextTTF(306, 72, 20, COL_WII_BLUE, "MII PREVIEW");


    GRRLIB_Rectangle(306, 104, 298, 2, COL_BORDER, true);

    // Draw the composited Mii face texture if loaded, otherwise show waiting prompt
    if (g_hasMii && g_avatar) {
        g_avatar->draw(430.0f, 205.0f, 1.3f);
    } else {
        drawTextTTF(385, 225, 16, COL_TEXT_LABEL, "Waiting for Mii...");
    }

    // 4. BOTTOM-LEFT LOGO SPACE (x=20, y=328, w=254, h=140)
    drawRoundedPanel(20, 328, 254, 140, 12.0f, COL_PANEL, COL_BORDER);
    if (g_logoTex) {
        GRRLIB_DrawImg(38, 320, g_logoTex, 0, 0.9f, 0.9f, 0xFFFFFFFF);
    }

    // 5. BOTTOM INSTRUCTION BAR (Controls & live stream status message)
    drawRoundedPanel(290, 390, 330, 78, 12.0f, COL_PANEL, COL_BORDER);
    drawTextTTF(308, 398, 16, g_hasMii ? COL_GREEN : COL_RED,
                "Press [A] to Save Mii to Plaza");
    drawTextTTF(308, 421, 16, g_hasMii ? (g_isPaused ? COL_AMBER : COL_WII_BLUE) : COL_TEXT_LABEL,
                g_isPaused ? "Press [B] to Resume Live Stream"
                           : "Press [B] to Freeze & Inspect Mii");
    drawTextTTF(308, 445, 14, g_isPaused ? COL_AMBER : COL_GREEN, "%s", g_statusMsg);
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
        // Create a non-blocking UDP socket bound to LISTEN_PORT (8080)
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

    // 3. Load TrueType Font, PNG Logo, initial Mii count, and MiiAvatar instance
    g_font      = GRRLIB_LoadTTF(font_ttf, font_ttf_size);
    g_logoTex   = GRRLIB_LoadTexturePNG(logo_png);
    g_totalMiis = getMiiCount();

    if (miiRenderReady) {
        g_avatar = new MiiAvatar();
    }

    // 4. Main 60 FPS Loop
    while (SYS_MainLoop()) {
        // Scan Wiimote inputs; exit cleanly to loader/menu on HOME button
        WPAD_ScanPads();
        u32 pressed = WPAD_ButtonsDown(0);
        if (pressed & WPAD_BUTTON_HOME) break;

        // Toggle Live Update / Freeze with [B]
        if ((pressed & WPAD_BUTTON_B) && g_hasMii) {
            g_isPaused = !g_isPaused;
            if (g_isPaused) {
                snprintf(g_statusMsg, sizeof(g_statusMsg), "Stream frozen.");
            } else {
                snprintf(g_statusMsg, sizeof(g_statusMsg), "Live updating resumed (Pkts: %u)", g_pktCount);
            }
        }

        // Drain UDP socket queue so live streaming never lags behind
        if (netActive && sock >= 0) {
            RawMiiData incoming;
            RawMiiData latestPacket;
            bool gotNewPacket = false;
            struct sockaddr_in client;
            u32 clientLen = sizeof(client);

            // Read all queued 74-byte UDP packets arrived this frame, keeping the newest one
            while (net_recvfrom(sock, &incoming, sizeof(RawMiiData), 0,
                                reinterpret_cast<struct sockaddr*>(&client), &clientLen) == sizeof(RawMiiData)) {
                latestPacket = incoming;
                gotNewPacket = true;
                g_pktCount++;
                clientLen = sizeof(client);
            }

            // Only update active Mii & rebuild face texture if NOT paused
            if (gotNewPacket && !g_isPaused) {
                // Avoid costly EFB texture rebuilds if the incoming 74 bytes are identical
                bool dataChanged = !g_hasMii || (memcmp(&g_receivedMii, &latestPacket, sizeof(RawMiiData)) != 0);
                g_receivedMii = latestPacket;
                g_hasMii = true;

                // Decode raw big-endian packet into renderer struct and mark avatar for EFB rebuild
                if (dataChanged && g_avatar) {
                    Mii decodedMii;
                    MiiDecodeRaw(reinterpret_cast<const u8*>(&g_receivedMii), &decodedMii);
                    g_avatar->setMii(decodedMii);
                }
                snprintf(g_statusMsg, sizeof(g_statusMsg), "Live updating... (Packet #%u)", g_pktCount);
            }
        }

        // Save to NAND Mii Plaza on [A]
        if ((pressed & WPAD_BUTTON_A) && g_hasMii) {
            char savedName[16] = "";
            if (saveToMiiPlaza(savedName, sizeof(savedName))) {
                g_totalMiis = getMiiCount(); // Refresh Plaza counter in top status bar
                snprintf(g_statusMsg, sizeof(g_statusMsg), "Saved '%s' to Mii Plaza!", savedName);
            } else {
                snprintf(g_statusMsg, sizeof(g_statusMsg), "Error: Could not write RFL_DB.dat");
            }
        }

        // IMPORTANT: Call avatar->update() FIRST before drawing the UI!
        // Off-screen EFB composition renders to the top-left of the framebuffer and clears it when dirty
        if (g_hasMii && g_avatar) {
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