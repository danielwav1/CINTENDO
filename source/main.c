#include <3ds.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

static const char *menuItems[] = {
    "Applications",
    "SD Card",
    "System",
    "Settings",
    "About"
};

static int showHelp = 1;

static void drawDirectory(const char *path)
{
    DIR *directory = opendir(path);
    if (!directory)
    {
        printf("Could not open directory.\n");
        printf("Check that the SD card is mounted.\n");
        return;
    }

    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(directory)) != NULL && count < 18)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        printf("%s\n", entry->d_name);
        count++;
    }

    if (count == 0)
        printf("This directory is empty.\n");

    closedir(directory);
}

static void drawTop(int page, int selected)
{
    printf("CINTENDO 1.0\n");
    printf("Your 3DS. Your way.\n");
    printf("--------------------------------\n");

    if (page == 0)
    {
        printf("HOME\n\n");

        for (int i = 0; i < 5; i++)
            printf("%s %s\n", selected == i ? ">" : " ", menuItems[i]);

        printf("\n");
        printf("made by danielwav1\n");
    }
    else if (page == 1)
    {
        printf("APPLICATIONS\n");
        printf("Files in sdmc:/3ds\n\n");
        drawDirectory("sdmc:/3ds");
        printf("\nApp launching is not available yet.\n");
    }
    else if (page == 2)
    {
        printf("SD CARD BROWSER\n");
        printf("Root directory: sdmc:/\n\n");
        drawDirectory("sdmc:/");
        printf("\nRead-only view.\n");
    }
    else if (page == 3)
    {
        printf("SYSTEM\n\n");
        printf("Console: Nintendo 3DS family\n");
        printf("Environment: Homebrew Launcher\n");
        printf("Application: CINTENDO 1.0\n");
        printf("Storage: SD card\n");
        printf("\n");
        printf("Use B to return home.\n");
    }
    else if (page == 4)
    {
        printf("SETTINGS\n\n");
        printf("%s Show control hints: %s\n", selected == 0 ? ">" : " ", showHelp ? "ON" : "OFF");
        printf("\nPress A to toggle.\n");
    }
    else if (page == 5)
    {
        printf("ABOUT CINTENDO\n\n");
        printf("CINTENDO 1.0\n");
        printf("A homebrew project for Nintendo 3DS.\n\n");
        printf("Made by danielwav1\n");
        printf("GitHub: danielwav1/CINTENDO\n");
    }
}

static void drawBottom(int page)
{
    printf("CONTROLS\n");
    printf("--------------------------------\n");

    if (showHelp)
    {
        printf("D-Pad  Navigate\n");
        printf("A      Select / Toggle\n");
        printf("B      Back\n");
        printf("START  Exit CINTENDO\n");
    }
    else
    {
        printf("Control hints are disabled.\n");
        printf("D-Pad: navigate | A: select\n");
        printf("B: back | START: exit\n");
    }

    printf("\n");

    if (page == 1)
        printf("Application list is view-only.\n");
    else if (page == 2)
        printf("SD card contents are view-only.\n");
    else
        printf("CINTENDO 1.0 - made by danielwav1\n");
}

int main(void)
{
    gfxInitDefault();

    PrintConsole topScreen;
    PrintConsole bottomScreen;

    consoleInit(GFX_TOP, &topScreen);
    consoleInit(GFX_BOTTOM, &bottomScreen);

    int page = 0;
    int selected = 0;
    int running = 1;
    int redraw = 1;

    while (aptMainLoop() && running)
    {
        hidScanInput();
        u32 keys = hidKeysDown();

        if (keys & KEY_START)
            running = 0;

        if (page == 0)
        {
            if (keys & KEY_DOWN)
            {
                selected = (selected + 1) % 5;
                redraw = 1;
            }

            if (keys & KEY_UP)
            {
                selected = (selected + 4) % 5;
                redraw = 1;
            }

            if (keys & KEY_A)
            {
                page = selected + 1;
                selected = 0;
                redraw = 1;
            }
        }
        else
        {
            if (keys & KEY_B)
            {
                page = 0;
                selected = 0;
                redraw = 1;
            }

            if (page == 4 && (keys & KEY_A))
            {
                showHelp = !showHelp;
                redraw = 1;
            }
        }

        if (redraw)
        {
            consoleSelect(&topScreen);
            consoleClear();
            drawTop(page, selected);

            consoleSelect(&bottomScreen);
            consoleClear();
            drawBottom(page);

            gfxFlushBuffers();
            gfxSwapBuffers();
            redraw = 0;
        }

        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
