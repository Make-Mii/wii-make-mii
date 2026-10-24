#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>
#include <grrlib.h>
#include "miirender.h"

int main() {
    GRRLIB_Init();
    WPAD_Init();
    if (!MiiRender_Init()) { GRRLIB_Exit(); return 1; }

    static Mii miis[100];

	// TODO: Import struct here once we have it ready to be used from AI model
    Nii myMii;

    MiiAvatar avatar;
    avatar.setMii(myMii);
    // Any other source works too:  Mii m; MiiDecodeRaw(my74Bytes, &m); avatar.setMii(m);

    while (1) {
        WPAD_ScanPads();
        u32 down = WPAD_ButtonsDown(0);

        if (down & WPAD_BUTTON_HOME) break;
        if (down & WPAD_BUTTON_A)    avatar.setExpression(MII_HAPPY);
        if (down & WPAD_BUTTON_B)    avatar.setExpression(MII_SAD);
        if (down & WPAD_BUTTON_1)    avatar.setExpression(MII_SHOCKED);
        if (down & WPAD_BUTTON_2)    avatar.setExpression(MII_MAD);
        if (down & WPAD_BUTTON_DOWN) avatar.setExpression(MII_NEUTRAL);
        if (count > 1 && (down & WPAD_BUTTON_PLUS))  { current = (current + 1) % count;         avatar.setMii(miis[current]); }
        if (count > 1 && (down & WPAD_BUTTON_MINUS)) { current = (current + count - 1) % count; avatar.setMii(miis[current]); }

        avatar.update();                          // 1. FIRST: may build the texture via the EFB
        GRRLIB_FillScreen(0x002088FF);            // 2. then draw the frame
        avatar.draw(320, 240, 1.5f);
        GRRLIB_Render();
    }

    MiiRender_Shutdown();
    GRRLIB_Exit();
    return 0;
}