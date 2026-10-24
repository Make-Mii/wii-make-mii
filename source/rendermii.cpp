/*===========================================
    RenderMii Example
    Code     : mdbrim

    rendermii.cpp
============================================*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wiiuse/wpad.h>
#include <ogcsys.h>
#include <gccore.h>
#include <math.h>
#include <fat.h>
#include <stdarg.h>

#include <FreeTypeGX.h>
#include <mii.h>                            
#include <grrlib.h>

#include "miidraw.h"

#include "pointer_png.h"
#include "arial_rounded_ttf.h"

extern "C" {
    int NoOfMiis = MII_MAX;
    extern int isfs_stage;
    extern int isfs_code;
}
extern int rawMiiCount;
extern char rawName0[11];

static FreeTypeGX* arial16;
static GRRLIB_texImg* img_pointer;
static GRRLIB_texImg* img_mii[5];
extern GRRLIB_texImg* img_staticmii[100];   // Stored static Miis
extern Mii* miis;
extern char miinames[100][11];

int selected[5] = {0, 0, 0, 0, 0};          // 5 slots displayed on screen

u32 wpaddown, wpadheld;
ir_t ir1, ir2, ir3;

void printf_ttf(FreeTypeGX* font, int xpos, int ypos, int shadow, GXColor fgc, GXColor bgc, u16 style, const char *s, ...) {
    if (!font) return;
    va_list ap;
    char line[200];
    wchar_t line_wchar[200];

    va_start(ap, s);
    vsnprintf(line, 200, s, ap);
    va_end(ap);

    char *src = line;
    wchar_t *dst = line_wchar;
    while((*dst++ = *src++));
    
    font->drawText(xpos + shadow, ypos + shadow, line_wchar, bgc, style);
    font->drawText(xpos, ypos, line_wchar, fgc, style);
}

int drawthemiis(int currentmii) {
    int themiis = 1;
    int x = -150;
    int y = -150;
    int i = 0;
    int j = 0;                         // Current page
    int n = 0;
    int selection = currentmii;        // Default fallback

    int maxPages = (NoOfMiis > 0) ? ((NoOfMiis - 1) / 15) : 0;
    int debounce = 0;                  // Prevent opening click from triggering immediate selection

    while(themiis == 1) {
        WPAD_SetVRes(0, 640 + 100, 480 + 100);
        WPAD_ScanPads();
        wpaddown = WPAD_ButtonsDown(0);
        WPAD_IR(WPAD_CHAN_0, &ir1);

        // 1. Clear background
        GRRLIB_FillScreen(0x000044FF);

        // 2. Header
        printf_ttf(arial16, 200, 25, 1, (GXColor){255, 255, 255, 255}, (GXColor){0, 0, 0, 255}, 
                   FTGX_JUSTIFY_LEFT, "SELECT A MII (PAGE %d / %d)", j + 1, maxPages + 1);

        // 3. Draw Grid Cards
        for(i = 0; i < 15; i++) {
            n = j * 15 + i;
            int boxX = 90 + ((i % 5) * 100);
            int boxY = 55 + ((i / 5) * 130);

            // Card background
            GRRLIB_Rectangle(boxX, boxY, 90, 95, 0x4982B299, true);

            // Selection hover border
            if (x == boxX && y == boxY) {
                GRRLIB_Rectangle(boxX, boxY, 90, 95, 0x00FF00FF, false);
                GRRLIB_Rectangle(boxX + 1, boxY + 1, 88, 93, 0x00FF00FF, false);
            }

            // Draw thumbnail and name safely
            if (n < NoOfMiis && n < 100) {
                if (img_staticmii[n] != NULL) {
                    GRRLIB_DrawImg(boxX - 5, boxY + 8, img_staticmii[n], 0, 0.5f, 0.5f, 0xFFFFFFFF);
                }

                char displayName[12];
                if (miinames[n][0] != '\0') {
                    snprintf(displayName, sizeof(displayName), "%.10s", miinames[n]);
                } else {
                    snprintf(displayName, sizeof(displayName), "Mii %d", n + 1);
                }

                // Left justify to avoid FTGX_JUSTIFY_CENTER calculation crashes
                printf_ttf(arial16, boxX + 10, boxY + 75, 1, 
                           (GXColor){255, 255, 255, 255}, (GXColor){0, 0, 0, 255}, 
                           FTGX_JUSTIFY_LEFT, "%s", displayName);
            }
        }

        // 4. Draw Pointer
        if (img_pointer != NULL) {
            GRRLIB_DrawImg(ir1.x - 40, ir1.y - 40, img_pointer, ir1.angle, 1.0f, 1.0f, 0xFFFFFFFF);
        }

        GRRLIB_Render();

        // Wait 10 frames before processing clicks to clear the button press that opened this menu
        if (debounce < 10) {
            debounce++;
            continue;
        }

        // 5. Hover and Click Detection
        x = -150;
        y = -150;
        for(i = 0; i < 15; i++) {
            int boxX = 90 + ((i % 5) * 100);
            int boxY = 55 + ((i / 5) * 130);

            if(GRRLIB_PtInRect(boxX, boxY, 90, 95, ir1.x, ir1.y)) {
                x = boxX;
                y = boxY;
                int target = j * 15 + i;
                if(target < NoOfMiis && (wpaddown & WPAD_BUTTON_A)) {
                    selection = target;
                    themiis = 0;
                    break;
                }
            }
        }

        // Cancel / Back out with B
        if(wpaddown & WPAD_BUTTON_B) {
            selection = currentmii;
            themiis = 0;
            break;
        }

        // Pagination
        if(((wpaddown & WPAD_BUTTON_RIGHT) || (wpaddown & WPAD_BUTTON_PLUS)) && (j < maxPages)) {
            j++;
        }
        if(((wpaddown & WPAD_BUTTON_LEFT) || (wpaddown & WPAD_BUTTON_MINUS)) && (j > 0)) {
            j--;
        }

        if(wpaddown & WPAD_BUTTON_HOME) {
            exit(0);
        }
    }

    return selection;
}

void initme() {
    GRRLIB_Init();
    GRRLIB_MiisInit();

    if (miis == NULL) {
        NoOfMiis = 0;
    }

    GRRLIB_CreateStaticMiis();

    img_pointer = GRRLIB_LoadTexture(pointer_png);

    for(int i = 0; i < 5; i++) {
        img_mii[i] = GRRLIB_CreateEmptyTexture(180, 200);
    }

    arial16 = new FreeTypeGX();
    arial16->loadFont(arial_rounded_ttf, arial_rounded_ttf_size, 16);

    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);
}

void randomizemii() {
    if (NoOfMiis <= 0) {
        for (int i = 0; i < 5; i++) selected[i] = 0;
        return;
    }
    for(int i = 0; i < 5; i++) {
        selected[i] = rand() % NoOfMiis;
    }
}

int main() {
    initme();

    // Default all 5 slots safely to existing Miis
    for(int i = 0; i < 5; i++) {
        selected[i] = (NoOfMiis > 0) ? (i % NoOfMiis) : 0;
    }

    int eyes[5] = {0, 0, 0, 0, 0};
    int mouth[5] = {0, 0, 0, 0, 0};

    while(1) {
        WPAD_SetVRes(0, 640 + 100, 480 + 100);
        WPAD_ScanPads();
        wpaddown = WPAD_ButtonsDown(0);
        WPAD_IR(WPAD_CHAN_0, &ir1);

        // 1. Build the active Mii textures FIRST into EFB scratchpad
        if (miis != NULL && NoOfMiis > 0) {
            for(int i = 0; i < 5; i++) {
                int mIdx = selected[i] % NoOfMiis;
                if (img_mii[i] != NULL) {
                    GRRLIB_BuildMii(miis[mIdx], mIdx, 1, eyes[i], mouth[i], img_mii[i]);
                }
            }
        }

        // 2. Clear Screen to bright blue AFTER building is finished
        GRRLIB_FillScreen(0x002088FF);

        // 3. Draw the 5 active Miis cleanly (scaleY = 1.0f puts them right-side up)
        if (miis != NULL && NoOfMiis > 0) {
            for(int i = 0; i < 5; i++) {
                if (img_mii[i] != NULL) {
                    GRRLIB_DrawImg(-10 + (i * 120), 90, img_mii[i], 0, 1.0f, 1.0f, 0xFFFFFFFF);
                }
            }
        }

        // 4. UI Button Labels
        printf_ttf(arial16, 175, 275, 1, (GXColor){255,255,255,255}, (GXColor){0,0,0,255}, FTGX_JUSTIFY_CENTER, "RANDOM HAPPY");
        printf_ttf(arial16, 320, 275, 1, (GXColor){255,255,255,255}, (GXColor){0,0,0,255}, FTGX_JUSTIFY_CENTER, "DEFAULT");
        printf_ttf(arial16, 465, 275, 1, (GXColor){255,255,255,255}, (GXColor){0,0,0,255}, FTGX_JUSTIFY_CENTER, "RANDOM SAD");
        printf_ttf(arial16, 175, 325, 1, (GXColor){255,255,255,255}, (GXColor){0,0,0,255}, FTGX_JUSTIFY_CENTER, "RANDOM SHOCK");
        printf_ttf(arial16, 465, 325, 1, (GXColor){255,255,255,255}, (GXColor){0,0,0,255}, FTGX_JUSTIFY_CENTER, "RANDOM MAD");
        printf_ttf(arial16, 320, 375, 1, (GXColor){255,255,255,255}, (GXColor){0,0,0,255}, FTGX_JUSTIFY_CENTER, "RANDOM MIIs");

        // 5. Button Click Handlers
        if(wpaddown & WPAD_BUTTON_A) {
            if(GRRLIB_PtInRect(100, 260, 150, 30, ir1.x, ir1.y)) {
                for(int i = 0; i < 5; i++) mouth[i] = MakeMiiHappy(1);
            }
            else if(GRRLIB_PtInRect(270, 260, 100, 30, ir1.x, ir1.y)) {
                for(int i = 0; i < 5; i++) { mouth[i] = 0; eyes[i] = 0; }
            }
            else if(GRRLIB_PtInRect(390, 260, 150, 30, ir1.x, ir1.y)) {
                for(int i = 0; i < 5; i++) mouth[i] = MakeMiiSad(1);
            }
            else if(GRRLIB_PtInRect(100, 310, 150, 30, ir1.x, ir1.y)) {
                for(int i = 0; i < 5; i++) mouth[i] = MakeMiiShocked(1);
            }
            else if(GRRLIB_PtInRect(390, 310, 150, 30, ir1.x, ir1.y)) {
                for(int i = 0; i < 5; i++) mouth[i] = MakeMiiMad(1);
            }
            else if(GRRLIB_PtInRect(240, 360, 160, 30, ir1.x, ir1.y)) {
                if (NoOfMiis > 0) {
                    for(int i = 0; i < 5; i++) selected[i] = rand() % NoOfMiis;
                }
            }
            // 6. Click on a Mii to open the picker grid
            for(int i = 0; i < 5; i++) {
                if(GRRLIB_PtInRect(20 + (i * 120), 90, 120, 160, ir1.x, ir1.y)) {
                    if (NoOfMiis > 0) {
                        int chosen = drawthemiis(selected[i] % NoOfMiis);
                        if (chosen >= 0 && chosen < NoOfMiis) {
                            selected[i] = chosen;
                        }
                    }
                    break;
                }
            }
        }

        // 7. Diagnostics Overlay
        printf_ttf(arial16, 25, 20, 1, (GXColor){255,255,0,255}, (GXColor){0,0,0,255}, FTGX_JUSTIFY_LEFT, 
                   "Miis Count: %d | miis ptr: %p", NoOfMiis, miis);

        GXColor diagCol = (isfs_stage == 6) ? (GXColor){100,255,100,255} : (GXColor){255,80,80,255};
        printf_ttf(arial16, 25, 45, 1, diagCol, (GXColor){0,0,0,255}, FTGX_JUSTIFY_LEFT, 
                   "ISFS Stage: %d | Code: %d", isfs_stage, isfs_code);

        // ---- ADD THIS BLOCK ----
        if (miis != NULL && NoOfMiis > 0) {
            const Mii &m = miis[selected[0] % NoOfMiis];
            printf_ttf(arial16, 25, 440, 1, (GXColor){255,255,255,255}, (GXColor){0,0,0,255},
                       FTGX_JUSTIFY_LEFT,
                       "eye v%d h%d sz%d | brow v%d h%d | lip v%d sz%d | nose v%d",
                       (int)m.eyeVertPos, (int)m.eyeHorizSpacing, (int)m.eyeSize,
                       (int)m.eyebrowVertPos, (int)m.eyebrowHorizSpacing,
                       (int)m.lipVertPos, (int)m.lipSize, (int)m.noseVertPos);
        }

        if (miis != NULL && NoOfMiis > 0) {
            const Mii &m = miis[0];
            printf_ttf(arial16, 25, 415, 1, (GXColor){255,255,255,255}, (GXColor){0,0,0,255},
                       FTGX_JUSTIFY_LEFT,
                       "raw n=%d libmii n=%d name0=%s | hair%d part%d glass%d",
                       rawMiiCount, NoOfMiis, rawName0,
                       (int)m.hairType, (int)m.hairPart, (int)m.glassesType);
        }

        // 8. Draw Pointer
        if (img_pointer != NULL) {
            GRRLIB_DrawImg(ir1.x - 40, ir1.y - 40, img_pointer, ir1.angle, 1.0f, 1.0f, 0xFFFFFFFF);
        }

        // 9. Send finished frame to screen
        GRRLIB_Render();

        if (wpaddown & WPAD_BUTTON_HOME) {
            exit(0);
        }
    }

    return 0;
}