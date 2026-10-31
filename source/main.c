
#include <3ds.h>
#include <stdio.h>

int main(void)
{
    gfxInitDefault();

    PrintConsole topScreen;
    PrintConsole bottomScreen;

    consoleInit(GFX_TOP, &topScreen);
    consoleInit(GFX_BOTTOM, &bottomScreen);

    const char *items[] = {
        "Applications",
        "Files",
        "System",
        "Settings",
        "About"
    };

    int selected = 0;
    int running = 1;

    while (aptMainLoop() && running)
    {
        hidScanInput();

        u32 keys = hidKeysDown();

        if (keys & KEY_DOWN)
            selected = (selected + 1) % 5;

        if (keys & KEY_UP)
            selected = (selected + 4) % 5;

        if (keys & KEY_START)
            running = 0;

        consoleSelect(&topScreen);
        consoleClear();

        printf("\x1b[2;13HCINTENDO");
        printf("\x1b[4;9HYour 3DS. Your way.");

        for (int i = 0; i < 5; i++)
        {
            printf("\x1b[%d;10H%s %s",
                8 + i * 2,
                selected == i ? ">" : " ",
                items[i]);
        }

        consoleSelect(&bottomScreen);
        consoleClear();

        printf("\x1b[3;5HCINTENDO SYSTEM");
        printf("\x1b[6;5HD-Pad  - Navigate");
        printf("\x1b[7;5HA      - Select");
        printf("\x1b[8;5HB      - Back");
        printf("\x1b[9;5HSTART  - Exit");

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}

