#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <3ds.h>

int main(int argc, char* argv[])
{
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);

	printf("Hello, world!\n");

	// Main loop
	while (aptMainLoop())
	{
		gspWaitForVBlank();
		gfxSwapBuffers();
		hidScanInput();

		// Your code goes here

#include <3ds.h>
#include <stdio.h>

int main(void)
{
    gfxInitDefault();

    PrintConsole topScreen;
    PrintConsole bottomScreen;

    consoleInit(GFX_TOP, &topScreen);
    consoleInit(GFX_BOTTOM, &bottomScreen);

    int selected = 0;

    const char *items[] = {
        "Applications",
        "Files",
        "System",
        "Settings",
        "About"
    };

    while (aptMainLoop())
    {
        hidScanInput();

        u32 kDown = hidKeysDown();

        if (kDown & KEY_DOWN)
            selected = (selected + 1) % 5;

        if (kDown & KEY_UP)
            selected = (selected + 4) % 5;

        if (kDown & KEY_START)
            break;

        consoleSelect(&topScreen);
        consoleClear();

        printf("\x1b[2;12HCINTENDO");
        printf("\x1b[4;8HYour 3DS. Your way.");

        for (int i = 0; i < 5; i++)
        {
            printf("\x1b[%d;10H%s %s",
                8 + i * 2,
                i == selected ? ">" : " ",
                items[i]);
        }

        consoleSelect(&bottomScreen);
        consoleClear();

        printf("\x1b[3;4HCINTENDO SYSTEM");
        printf("\x1b[6;4HD-Pad: Navigate");
        printf("\x1b[7;4HA: Select");
        printf("\x1b[8;4HB: Back");
        printf("\x1b[9;4HSTART: Exit");

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}

