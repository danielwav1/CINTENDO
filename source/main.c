#include <3ds.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#define MAX_ENTRIES 256
#define VISIBLE_ROWS 17
#define PATH_SIZE 512

typedef struct
{
    char name[256];
    unsigned long long size;
    int isDirectory;
} FileEntry;

typedef enum
{
    PAGE_HOME,
    PAGE_FILES,
    PAGE_APPS,
    PAGE_SYSTEM,
    PAGE_SETTINGS,
    PAGE_ABOUT,
    PAGE_DETAIL
} Page;

static PrintConsole topScreen;
static PrintConsole bottomScreen;
static FileEntry entries[MAX_ENTRIES];
static int entryCount;
static int selected;
static int scrollOffset;
static int showHints = 1;
static int accent = 0;
static int running = 1;
static int dirty = 1;
static Page page = PAGE_HOME;
static char currentPath[PATH_SIZE] = "sdmc:/";
static char detailPath[PATH_SIZE];
static char statusMessage[96] = "Welcome to CINTENDO.";

static const char *homeItems[] = {
    "File Manager",
    "Homebrew Library",
    "System Dashboard",
    "Settings",
    "About CINTENDO"
};

static const char *accentCodes[] = {
    "\x1b[36m",
    "\x1b[35m",
    "\x1b[32m"
};

static const char *resetCode = "\x1b[0m";

static void setStatus(const char *message)
{
    snprintf(statusMessage, sizeof(statusMessage), "%s", message);
}

static void loadSettings(void)
{
    FILE *file = fopen("sdmc:/3ds/CINTENDO/config.ini", "r");
    if (!file)
        return;

    int hints = showHints;
    int color = accent;

    if (fscanf(file, "%d %d", &hints, &color) == 2)
    {
        showHints = hints ? 1 : 0;
        accent = color >= 0 && color <= 2 ? color : 0;
    }

    fclose(file);
}

static void saveSettings(void)
{
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/CINTENDO", 0777);

    FILE *file = fopen("sdmc:/3ds/CINTENDO/config.ini", "w");
    if (!file)
    {
        setStatus("Could not save settings to the SD card.");
        return;
    }

    fprintf(file, "%d %d\n", showHints, accent);
    fclose(file);
    setStatus("Settings saved.");
}

static int compareEntries(const void *left, const void *right)
{
    const FileEntry *a = left;
    const FileEntry *b = right;

    if (a->isDirectory != b->isDirectory)
        return b->isDirectory - a->isDirectory;

    return strcasecmp(a->name, b->name);
}

static int hasExtension(const char *name, const char *extension)
{
    size_t nameLength = strlen(name);
    size_t extensionLength = strlen(extension);

    if (nameLength < extensionLength)
        return 0;

    return strcasecmp(name + nameLength - extensionLength, extension) == 0;
}

static int scanDirectory(const char *path, int applicationsOnly)
{
    DIR *directory = opendir(path);
    entryCount = 0;

    if (!directory)
    {
        setStatus("Could not read this directory.");
        return 0;
    }

    struct dirent *item;

    while ((item = readdir(directory)) != NULL && entryCount < MAX_ENTRIES)
    {
        if (strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0)
            continue;

        char fullPath[PATH_SIZE];
        struct stat info;

        if (snprintf(fullPath, sizeof(fullPath), "%s%s%s", path,
                     path[strlen(path) - 1] == '/' ? "" : "/", item->d_name) >= (int)sizeof(fullPath))
            continue;

        if (stat(fullPath, &info) != 0)
            continue;

        int isDirectory = S_ISDIR(info.st_mode);

        if (applicationsOnly && (isDirectory || !hasExtension(item->d_name, ".3dsx")))
            continue;

        FileEntry *entry = &entries[entryCount++];
        snprintf(entry->name, sizeof(entry->name), "%s", item->d_name);
        entry->isDirectory = isDirectory;
        entry->size = isDirectory ? 0 : (unsigned long long)info.st_size;
    }

    closedir(directory);
    qsort(entries, entryCount, sizeof(FileEntry), compareEntries);
    selected = 0;
    scrollOffset = 0;
    return 1;
}

static void formatSize(unsigned long long bytes, char *output, size_t outputSize)
{
    if (bytes >= 1024ULL * 1024ULL * 1024ULL)
        snprintf(output, outputSize, "%.2f GB", (double)bytes / 1073741824.0);
    else if (bytes >= 1024ULL * 1024ULL)
        snprintf(output, outputSize, "%.1f MB", (double)bytes / 1048576.0);
    else if (bytes >= 1024ULL)
        snprintf(output, outputSize, "%.1f KB", (double)bytes / 1024.0);
    else
        snprintf(output, outputSize, "%llu B", bytes);
}

static void drawHeader(const char *section)
{
    printf("%sCINTENDO%s  %s\n", accentCodes[accent], resetCode, section);
    printf("----------------------------------------------\n");
}

static void drawHome(void)
{
    consoleSelect(&topScreen);
    consoleClear();
    drawHeader("HOME");

    printf("\n");
    printf("  Your 3DS. Your way.\n\n");

    for (int i = 0; i < 5; i++)
        printf("%s %s\n", selected == i ? ">" : " ", homeItems[i]);

    printf("\n");
    printf("  Made by danielwav1\n");

    consoleSelect(&bottomScreen);
    consoleClear();
    printf("QUICK INFO\n");
    printf("--------------------------------------\n");
    printf("  File Manager  Browse your SD card\n");
    printf("  Homebrew      Find your .3dsx apps\n");
    printf("  Dashboard     Storage information\n\n");
    printf("STATUS\n");
    printf("%s\n", statusMessage);
}

static void drawFileList(int applicationsOnly)
{
    consoleSelect(&topScreen);
    consoleClear();
    drawHeader(applicationsOnly ? "HOMEBREW LIBRARY" : "FILE MANAGER");

    printf("\n");
    printf("%.43s\n", currentPath);
    printf("----------------------------------------------\n");

    int visible = 0;

    if (!applicationsOnly && strcmp(currentPath, "sdmc:/") != 0)
    {
        printf("%s ..  [Parent directory]\n", selected == 0 ? ">" : " ");
        visible++;
    }

    int firstEntry = scrollOffset;
    for (int i = firstEntry; i < entryCount && visible < VISIBLE_ROWS; i++)
    {
        int rowSelected = selected == i + ((!applicationsOnly && strcmp(currentPath, "sdmc:/") != 0) ? 1 : 0);
        char sizeText[24];

        if (entries[i].isDirectory)
            snprintf(sizeText, sizeof(sizeText), "<DIR>");
        else
            formatSize(entries[i].size, sizeText, sizeof(sizeText));

        printf("%s %-29.29s %8s\n",
               rowSelected ? ">" : " ",
               entries[i].name,
               sizeText);
        visible++;
    }

    if (entryCount == 0)
        printf("\n  No matching files found.\n");

    consoleSelect(&bottomScreen);
    consoleClear();
    printf("%s\n", applicationsOnly ? "HOMEBREW LIBRARY" : "FILE MANAGER");
    printf("--------------------------------------\n");
    printf("  %d entries found\n", entryCount);
    printf("  A: Open / details\n");
    printf("  B: %s\n", applicationsOnly ? "Home" : "Parent folder");
    printf("  UP/DOWN: Navigate\n");
    printf("\n%s\n", statusMessage);
    printf("\n  made by danielwav1\n");
}

static void drawSystem(void)
{
    struct statvfs storage;
    unsigned long long total = 0;
    unsigned long long freeSpace = 0;

    if (statvfs("sdmc:/", &storage) == 0)
    {
        total = (unsigned long long)storage.f_blocks * storage.f_frsize;
        freeSpace = (unsigned long long)storage.f_bavail * storage.f_frsize;
    }

    char totalText[24];
    char freeText[24];
    formatSize(total, totalText, sizeof(totalText));
    formatSize(freeSpace, freeText, sizeof(freeText));

    consoleSelect(&topScreen);
    consoleClear();
    drawHeader("SYSTEM DASHBOARD");

    printf("\n  DEVICE\n");
    printf("  Nintendo 3DS family\n");
    printf("  Application: CINTENDO 1.0\n");
    printf("  Runtime: Homebrew Launcher\n\n");
    printf("  SD CARD\n");
    printf("  Total capacity: %s\n", totalText);
    printf("  Available:      %s\n", freeText);
    printf("  Used:           %s\n", total >= freeSpace ? totalText : "Unknown");
    printf("\n  Current location\n");
    printf("  %.42s\n", currentPath);

    consoleSelect(&bottomScreen);
    consoleClear();
    printf("SYSTEM DASHBOARD\n");
    printf("--------------------------------------\n");
    printf("  Storage values are read from the SD.\n");
    printf("  No system settings are changed.\n\n");
    printf("%s\n", statusMessage);
    printf("\n  B: Home");
}

static void drawSettings(void)
{
    consoleSelect(&topScreen);
    consoleClear();
    drawHeader("SETTINGS");

    printf("\n");
    printf("%s Control hints: %s\n", selected == 0 ? ">" : " ", showHints ? "ON" : "OFF");
    printf("%s Accent color:  %s\n", selected == 1 ? ">" : " ",
           accent == 0 ? "Cyan" : accent == 1 ? "Magenta" : "Green");
    printf("\n  Changes are saved to the SD card.\n");

    consoleSelect(&bottomScreen);
    consoleClear();
    printf("SETTINGS\n");
    printf("--------------------------------------\n");
    printf("  UP/DOWN: Select setting\n");
    printf("  A: Change value\n");
    printf("  B: Home\n\n");
    printf("%s\n", statusMessage);
}

static void drawAbout(void)
{
    consoleSelect(&topScreen);
    consoleClear();
    drawHeader("ABOUT");

    printf("\n  CINTENDO 1.0\n");
    printf("  A homebrew companion for Nintendo 3DS.\n\n");
    printf("  Made by danielwav1\n");
    printf("  github.com/danielwav1/CINTENDO\n\n");
    printf("  Built with devkitPro and libctru.\n");

    consoleSelect(&bottomScreen);
    consoleClear();
    printf("ABOUT CINTENDO\n");
    printf("--------------------------------------\n");
    printf("  Browse files, inspect homebrew apps,\n");
    printf("  and check SD card storage.\n\n");
    printf("  CINTENDO is independent homebrew\n");
    printf("  software and is not official Nintendo\n");
    printf("  software.\n");
}

static void drawDetail(void)
{
    struct stat info;
    char sizeText[24];
    int exists = stat(detailPath, &info) == 0;

    consoleSelect(&topScreen);
    consoleClear();
    drawHeader("FILE DETAILS");

    printf("\n  Name\n  %.42s\n\n", strrchr(detailPath, '/') ? strrchr(detailPath, '/') + 1 : detailPath);
    printf("  Full path\n  %.42s\n\n", detailPath);

    if (exists)
    {
        formatSize((unsigned long long)info.st_size, sizeText, sizeof(sizeText));
        printf("  Type: %s\n", S_ISDIR(info.st_mode) ? "Directory" : "File");
        printf("  Size: %s\n", S_ISDIR(info.st_mode) ? "—" : sizeText);
    }
    else
        printf("  This item is no longer available.\n");

    if (hasExtension(detailPath, ".3dsx"))
        printf("\n  Homebrew executable (.3dsx)\n");

    consoleSelect(&bottomScreen);
    consoleClear();
    printf("FILE DETAILS\n");
    printf("--------------------------------------\n");
    printf("  A .3dsx file can be opened from the\n");
    printf("  Homebrew Launcher. CINTENDO does not\n");
    printf("  chainload applications yet.\n\n");
    printf("  B: Back");
}

static void draw(void)
{
    if (page == PAGE_HOME)
        drawHome();
    else if (page == PAGE_FILES)
        drawFileList(0);
    else if (page == PAGE_APPS)
        drawFileList(1);
    else if (page == PAGE_SYSTEM)
        drawSystem();
    else if (page == PAGE_SETTINGS)
        drawSettings();
    else if (page == PAGE_ABOUT)
        drawAbout();
    else if (page == PAGE_DETAIL)
        drawDetail();

    gfxFlushBuffers();
    gfxSwapBuffers();
}

static int selectedEntryIndex(int applicationsOnly)
{
    int parentOffset = !applicationsOnly && strcmp(currentPath, "sdmc:/") != 0;
    return selected - parentOffset;
}

static void openSelected(int applicationsOnly)
{
    int parentOffset = !applicationsOnly && strcmp(currentPath, "sdmc:/") != 0;

    if (parentOffset && selected == 0)
    {
        char *lastSlash = strrchr(currentPath, '/');
        if (lastSlash && lastSlash > currentPath + 5)
            *lastSlash = '\0';
        else
            snprintf(currentPath, sizeof(currentPath), "sdmc:/");

        scanDirectory(currentPath, 0);
        setStatus("Opened parent directory.");
        dirty = 1;
        return;
    }

    int index = selectedEntryIndex(applicationsOnly);
    if (index < 0 || index >= entryCount)
        return;

    char path[PATH_SIZE];
    if (snprintf(path, sizeof(path), "%s%s%s", currentPath,
                 currentPath[strlen(currentPath) - 1] == '/' ? "" : "/",
                 entries[index].name) >= (int)sizeof(path))
    {
        setStatus("Path is too long.");
        dirty = 1;
        return;
    }

    if (!applicationsOnly && entries[index].isDirectory)
    {
        snprintf(currentPath, sizeof(currentPath), "%s", path);
        scanDirectory(currentPath, 0);
        setStatus("Directory opened.");
        dirty = 1;
        return;
    }

    snprintf(detailPath, sizeof(detailPath), "%s", path);
    page = PAGE_DETAIL;
    dirty = 1;
}

static void moveSelection(int direction, int total)
{
    if (total <= 0)
    {
        selected = 0;
        return;
    }

    selected += direction;

    if (selected < 0)
        selected = total - 1;
    if (selected >= total)
        selected = 0;

    if (selected < scrollOffset)
        scrollOffset = selected;
    if (selected >= scrollOffset + VISIBLE_ROWS)
        scrollOffset = selected - VISIBLE_ROWS + 1;
}

int main(void)
{
    gfxInitDefault();
    consoleInit(GFX_TOP, &topScreen);
    consoleInit(GFX_BOTTOM, &bottomScreen);
    loadSettings();

    while (aptMainLoop() && running)
    {
        hidScanInput();
        u32 keys = hidKeysDown();

        if (keys & KEY_START)
            running = 0;

        if (page == PAGE_HOME)
        {
            if (keys & KEY_DOWN)
            {
                moveSelection(1, 5);
                dirty = 1;
            }
            if (keys & KEY_UP)
            {
                moveSelection(-1, 5);
                dirty = 1;
            }
            if (keys & KEY_A)
            {
                if (selected == 0)
                {
                    snprintf(currentPath, sizeof(currentPath), "sdmc:/");
                    scanDirectory(currentPath, 0);
                    page = PAGE_FILES;
                }
                else if (selected == 1)
                {
                    snprintf(currentPath, sizeof(currentPath), "sdmc:/3ds");
                    scanDirectory(currentPath, 1);
                    page = PAGE_APPS;
                }
                else if (selected == 2)
                    page = PAGE_SYSTEM;
                else if (selected == 3)
                {
                    selected = 0;
                    page = PAGE_SETTINGS;
                }
                else
                    page = PAGE_ABOUT;

                if (page != PAGE_SETTINGS)
                    selected = 0;
                dirty = 1;
            }
        }
        else if (page == PAGE_FILES || page == PAGE_APPS)
        {
            int parentOffset = page == PAGE_FILES && strcmp(currentPath, "sdmc:/") != 0;
            int total = entryCount + parentOffset;

            if (keys & KEY_DOWN)
            {
                moveSelection(1, total);
                dirty = 1;
            }
            if (keys & KEY_UP)
            {
                moveSelection(-1, total);
                dirty = 1;
            }
            if (keys & KEY_A)
                openSelected(page == PAGE_APPS);
            if (keys & KEY_B)
            {
                if (page == PAGE_APPS)
                    page = PAGE_HOME;
                else if (strcmp(currentPath, "sdmc:/") == 0)
                    page = PAGE_HOME;
                else
                {
                    char *lastSlash = strrchr(currentPath, '/');
                    if (lastSlash && lastSlash > currentPath + 5)
                        *lastSlash = '\0';
                    else
                        snprintf(currentPath, sizeof(currentPath), "sdmc:/");
                    scanDirectory(currentPath, 0);
                }
                selected = 0;
                scrollOffset = 0;
                dirty = 1;
            }
        }
        else if (page == PAGE_SETTINGS)
        {
            if (keys & KEY_UP)
            {
                selected = (selected + 1) % 2;
                dirty = 1;
            }
            if (keys & KEY_DOWN)
            {
                selected = (selected + 1) % 2;
                dirty = 1;
            }
            if (keys & KEY_A)
            {
                if (selected == 0)
                    showHints = !showHints;
                else
                    accent = (accent + 1) % 3;
                saveSettings();
                dirty = 1;
            }
            if (keys & KEY_B)
            {
                page = PAGE_HOME;
                selected = 0;
                dirty = 1;
            }
        }
        else
        {
            if (keys & KEY_B)
            {
                if (page == PAGE_DETAIL)
                    page = detailPath[0] ? (strstr(detailPath, "sdmc:/3ds") == detailPath ? PAGE_APPS : PAGE_FILES) : PAGE_FILES;
                else
                    page = PAGE_HOME;
                dirty = 1;
            }
        }

        if (dirty)
        {
            draw();
            dirty = 0;
        }

        if (showHints)
            hidScanInput();

        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
