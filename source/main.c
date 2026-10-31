#include <3ds.h>
#include <citro2d.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include "plugin_engine.h"

#define MAX_ENTRIES 220
#define PATH_SIZE 512
#define VISIBLE_ROWS 16

typedef struct
{
    char name[256];
    unsigned long long size;
    int directory;
} Entry;

typedef struct
{
    const char *name;
    const char *primary;
    const char *secondary;
    const char *accent;
    const char *tagline;
} Theme;

typedef enum
{
    HOME,
    FILES,
    APPS,
    VAULT,
    SYSTEM,
    SETTINGS,
    THEMES,
    ABOUT,
    DETAILS,
    ADVANCED,
    PLUGINS,
    PLUGIN_DETAILS,
    RESET_CONFIRM,
    MUSIC
} Page;

static const Theme themes[] = {
    {"Standard", "\x1b[31m", "\x1b[34m", "\x1b[37m", "Red / Blue / Black / White"},
    {"Aesthetic Pink", "\x1b[35m", "\x1b[32m", "\x1b[36m", "Pink / Green / Mint / Sprinkles"},
    {"CindraOS", "\x1b[31m", "\x1b[37m", "\x1b[37m", "Crimson / Black / White"},
    {"Midnight", "\x1b[35m", "\x1b[36m", "\x1b[37m", "Purple / Cyan / Midnight"},
    {"Ocean", "\x1b[36m", "\x1b[34m", "\x1b[37m", "Aqua / Blue / Ice"},
    {"Sunset", "\x1b[33m", "\x1b[35m", "\x1b[37m", "Amber / Pink / Dusk"}
};

static const char *reset = "\x1b[0m";
static const char *black = "\x1b[30m";
static PrintConsole top;
static PrintConsole bottom;
static C2D_TextBuf launcherText;
static Entry entries[MAX_ENTRIES];
static int count;
static int selected;
static int scroll;
static int themeIndex;
static int hints = 1;
static int darkMode = 1;
static int musicCount;
static int musicPlaying;
static int musicPaused;
static char musicFiles[128][256];
static void *musicData;
static ndspWaveBuf musicWave;
static int advancedUnlocked;
static int running = 1;
static int dirty = 1;
static int comboWasHeld;
static Page page = HOME;
static Page previousPage = FILES;
static char path[PATH_SIZE] = "sdmc:/";
static char detail[PATH_SIZE];
static char status[100] = "Welcome to CINTENDO.";
typedef CintendoPlugin Plugin;
static Plugin plugins[CINTENDO_PLUGIN_LIMIT];
static int pluginCount;
static Plugin activePlugin;
static char pluginResult[160] = "Select a plugin action.";

static const char *homeLabels[] = {
    "[FM]  File Manager",
    "[HB]  Homebrew Library",
    "[SV]  Save Vault",
    "[TH]  Theme Studio",
    "[SYS] System Dashboard",
    "[SET] Settings",
    "[ADV] Advanced Tools",
    "[i]   About CINTENDO",
    "[♫]   CINTENDO Music"
};


static void statusSet(const char *value);
static void ensureFolders(void);
static int hasExtension(const char *name, const char *extension);
static void scanMusic(void);
static void drawMusic(void);
static void stopMusic(void);
static int playMusicFile(const char *name);
static void settingsSave(void);
static void header(const char *section);
static void footer(void);

static void scanPlugins(void)
{
    pluginCount = pluginEngineScan(plugins, CINTENDO_PLUGIN_LIMIT);
}

static void resetCintendo(void)
{
    remove("sdmc:/3ds/CINTENDO/config.ini");
    DIR *dir = opendir("sdmc:/3ds/CINTENDO/PluginKeys");
    if (dir)
    {
        struct dirent *item;
        while ((item = readdir(dir)))
        {
            if (!hasExtension(item->d_name, ".active"))
                continue;
            char full[PATH_SIZE];
            if (snprintf(full, sizeof(full), "sdmc:/3ds/CINTENDO/PluginKeys/%s", item->d_name) < (int)sizeof(full))
                remove(full);
        }
        closedir(dir);
    }
    themeIndex = 0;
    hints = 1;
    darkMode = 1;
    advancedUnlocked = 0;
    selected = 0;
    scroll = 0;
    scanPlugins();
    statusSet("Settings reset. Plugin keys and Save Vault backups were preserved.");
    settingsSave();
}

static void drawPlugins(void)
{
    scanPlugins();
    consoleSelect(&top);
    consoleClear();
    header("PLUGIN KEY CENTER");
    printf("\n  Found keys: %d / %d\n\n", pluginCount, CINTENDO_PLUGIN_LIMIT);
    for (int i = 0; i < pluginCount && i < 14; i++)
        printf("  %s [%s] %s\n", selected == i ? ">" : " ", plugins[i].enabled ? "ON " : "OFF", plugins[i].name);
    if (!pluginCount)
        printf("  No matching key + script pairs found.\n");
    consoleSelect(&bottom);
    consoleClear();
    header("PLUGIN CONTROLS");
    printf("  Keys: PluginKeys/*.cplug\n");
    printf("  Code: Plugins/*.cscript\n\n");
    printf("  A: Enable / disable key\n  X: Run enabled plugin\n  B: Advanced Tools");
    footer();
}

static void drawPluginDetails(void)
{
    consoleSelect(&top);
    consoleClear();
    header("PLUGIN SCRIPT");
    printf("\n  %s\n\n", activePlugin.name);
    printf("  ID: %s\n", activePlugin.id);
    printf("  Status: %s\n\n", activePlugin.enabled ? "ENABLED" : "DISABLED");
    printf("  %s\n", activePlugin.description);
    consoleSelect(&bottom);
    consoleClear();
    header("PLUGIN OUTPUT");
    printf("  %s\n\n", pluginResult);
    printf("  A: Enable / disable\n  X: Execute script\n  B: Back");
    footer();
}

static void statusSet(const char *value);
static void ensureFolders(void);
static int hasExtension(const char *name, const char *extension);

static u16 musicRead16(const unsigned char *p)
{
    return (u16)(p[0] | ((u16)p[1] << 8));
}

static u32 musicRead32(const unsigned char *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static void stopMusic(void)
{
    ndspChnReset(0);
    if (musicData) { linearFree(musicData); musicData = NULL; }
    musicPlaying = 0;
    musicPaused = 0;
}

static int playMusicFile(const char *name)
{
    char full[PATH_SIZE];
    if (snprintf(full, sizeof(full), "sdmc:/3ds/CINTENDO/Music/%s", name) >= (int)sizeof(full)) return 0;
    FILE *file = fopen(full, "rb");
    if (!file) return 0;
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return 0; }
    long length = ftell(file);
    if (length < 44 || length > 8 * 1024 * 1024) { fclose(file); return 0; }
    rewind(file);
    unsigned char *bytes = malloc((size_t)length);
    if (!bytes) { fclose(file); return 0; }
    if (fread(bytes, 1, (size_t)length, file) != (size_t)length) { free(bytes); fclose(file); return 0; }
    fclose(file);
    if (memcmp(bytes, "RIFF", 4) || memcmp(bytes + 8, "WAVE", 4)) { free(bytes); return 0; }
    u16 format = 0, channels = 0, bits = 0;
    u32 rate = 0, dataSize = 0;
    unsigned char *data = NULL;
    for (u32 offset = 12; offset + 8 <= (u32)length;)
    {
        u32 chunkSize = musicRead32(bytes + offset + 4);
        u32 chunkData = offset + 8;
        if (chunkData + chunkSize > (u32)length) break;
        if (!memcmp(bytes + offset, "fmt ", 4) && chunkSize >= 16)
        {
            format = musicRead16(bytes + chunkData);
            channels = musicRead16(bytes + chunkData + 2);
            rate = musicRead32(bytes + chunkData + 4);
            bits = musicRead16(bytes + chunkData + 14);
        }
        else if (!memcmp(bytes + offset, "data", 4)) { data = bytes + chunkData; dataSize = chunkSize; }
        offset = chunkData + chunkSize + (chunkSize & 1);
    }
    if (format != 1 || !data || !rate || (channels != 1 && channels != 2) || (bits != 8 && bits != 16))
    {
        free(bytes);
        return 0;
    }
    u32 samples = dataSize / (channels * (bits / 8));
    if (!samples) { free(bytes); return 0; }
    stopMusic();
    musicData = linearAlloc(dataSize);
    if (!musicData) { free(bytes); return 0; }
    memcpy(musicData, data, dataSize);
    free(bytes);
    memset(&musicWave, 0, sizeof(musicWave));
    musicWave.data_vaddr = musicData;
    musicWave.nsamples = samples;
    musicWave.looping = false;
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, (float)rate);
    ndspChnSetFormat(0, channels == 1 ? (bits == 8 ? NDSP_FORMAT_MONO_PCM8 : NDSP_FORMAT_MONO_PCM16) : (bits == 8 ? NDSP_FORMAT_STEREO_PCM8 : NDSP_FORMAT_STEREO_PCM16));
    ndspChnSetPaused(0, false);
    ndspChnWaveBufAdd(0, &musicWave);
    musicPlaying = 1;
    musicPaused = 0;
    return 1;
}

static void scanMusic(void)
{
    DIR *dir = opendir("sdmc:/3ds/CINTENDO/Music");
    musicCount = 0;
    if (!dir) return;
    struct dirent *item;
    while ((item = readdir(dir)) && musicCount < 128)
        if (hasExtension(item->d_name, ".wav"))
            snprintf(musicFiles[musicCount++], sizeof(musicFiles[0]), "%s", item->d_name);
    closedir(dir);
    selected = 0;
    scroll = 0;
}

static void drawMusic(void)
{
    consoleSelect(&top);
    consoleClear();
    header("CINTENDO MUSIC");
    printf("\n  Local music library\n  sdmc:/3ds/CINTENDO/Music/\n\n");
    for (int i = 0; i < musicCount && i < 14; i++)
        printf("%s %s\n", selected == i ? ">" : " ", musicFiles[i]);
    if (!musicCount) printf("  No WAV files found.\n");
    consoleSelect(&bottom);
    consoleClear();
    header("PLAYER");
    printf("  Status: %s\n", !musicPlaying ? "Stopped" : musicPaused ? "Paused" : "Playing");
    printf("\n  A: Play selected\n  X: Pause / resume\n  Y: Stop\n  B: Home\n\n");
    printf("  PCM WAV only: 8/16-bit, mono/stereo\n  Maximum file size: 8 MB");
    footer();
}

static void statusSet(const char *value)
{
    snprintf(status, sizeof(status), "%s", value);
}

static void ensureFolders(void)
{
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/CINTENDO", 0777);
    mkdir("sdmc:/3ds/CINTENDO/SaveVault", 0777);
    mkdir("sdmc:/3ds/CINTENDO/Plugins", 0777);
    mkdir("sdmc:/3ds/CINTENDO/PluginKeys", 0777);
    mkdir("sdmc:/3ds/CINTENDO/Music", 0777);
}

static void settingsLoad(void)
{
    FILE *file = fopen("sdmc:/3ds/CINTENDO/config.ini", "r");
    if (!file)
        return;

    int savedTheme = 0;
    int savedHints = 1;
    int savedDarkMode = 1;
    int fields = fscanf(file, "%d %d %d", &savedTheme, &savedHints, &savedDarkMode);
    if (fields >= 2)
    {
        if (savedTheme >= 0 && savedTheme < (int)(sizeof(themes) / sizeof(themes[0])))
            themeIndex = savedTheme;
        hints = savedHints ? 1 : 0;
        darkMode = fields >= 3 ? (savedDarkMode ? 1 : 0) : 1;
    }
    fclose(file);
}

static void settingsSave(void)
{
    ensureFolders();
    FILE *file = fopen("sdmc:/3ds/CINTENDO/config.ini", "w");
    if (!file)
    {
        statusSet("Could not save settings.");
        return;
    }
    fprintf(file, "%d %d %d\n", themeIndex, hints, darkMode);
    fclose(file);
    statusSet("Settings saved to SD card.");
}

static int hasExtension(const char *name, const char *extension)
{
    size_t a = strlen(name);
    size_t b = strlen(extension);
    return a >= b && strcasecmp(name + a - b, extension) == 0;
}

static int compareEntries(const void *left, const void *right)
{
    const Entry *a = left;
    const Entry *b = right;
    if (a->directory != b->directory)
        return b->directory - a->directory;
    return strcasecmp(a->name, b->name);
}

static int scan(const char *folder, int onlyApps)
{
    DIR *dir = opendir(folder);
    count = 0;
    if (!dir)
    {
        statusSet("Folder could not be opened.");
        return 0;
    }

    struct dirent *item;
    while ((item = readdir(dir)) && count < MAX_ENTRIES)
    {
        if (!strcmp(item->d_name, ".") || !strcmp(item->d_name, ".."))
            continue;

        char full[PATH_SIZE];
        struct stat info;
        int length = snprintf(full, sizeof(full), "%s%s%s", folder,
                              folder[strlen(folder) - 1] == '/' ? "" : "/",
                              item->d_name);
        if (length < 0 || length >= (int)sizeof(full) || stat(full, &info) != 0)
            continue;

        int isDir = S_ISDIR(info.st_mode);
        if (onlyApps && (isDir || !hasExtension(item->d_name, ".3dsx")))
            continue;

        snprintf(entries[count].name, sizeof(entries[count].name), "%s", item->d_name);
        entries[count].directory = isDir;
        entries[count].size = isDir ? 0 : (unsigned long long)info.st_size;
        count++;
    }
    closedir(dir);
    qsort(entries, count, sizeof(Entry), compareEntries);
    selected = 0;
    scroll = 0;
    return 1;
}

static void formatSize(unsigned long long value, char *out, size_t size)
{
    if (value >= 1073741824ULL)
        snprintf(out, size, "%.2f GB", (double)value / 1073741824.0);
    else if (value >= 1048576ULL)
        snprintf(out, size, "%.1f MB", (double)value / 1048576.0);
    else if (value >= 1024ULL)
        snprintf(out, size, "%.1f KB", (double)value / 1024.0);
    else
        snprintf(out, size, "%llu B", value);
}

static void header(const char *section)
{
    reset = darkMode ? "\x1b[0m" : "\x1b[30;47m";
    printf("%s", reset);
    printf("%sCINTENDO%s  %s%s\n", themes[themeIndex].primary, reset,
           themes[themeIndex].secondary, section);
    printf("%s==============================================%s\n", themes[themeIndex].accent, reset);
}

static void footer(void)
{
    printf("\n%sMade by danielwav1%s", themes[themeIndex].primary, reset);
}

static void launcherTextDraw(const char *value, float x, float y, float scale)
{
    C2D_Text text;
    C2D_TextBufClear(launcherText);
    C2D_TextParse(&text, launcherText, value);
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_AlignCenter, x, y, 0.5f, scale, scale);
}

static void drawHome(void)
{
    static const char *labels[] = {
        "Files", "Homebrew", "Save Vault",
        "Themes", "System", "Settings",
        "Plugins", "About", "Music"
    };
    static const char *symbols[] = {"F", "H", "S", "T", "D", "!", "P", "i", "M"};
    static const u32 colors[] = {
        0xFF8A42E8, 0xFF36A5D9, 0xFF36B58B,
        0xFFDB568B, 0xFF4774D8, 0xFF657080,
        0xFFB84DDB, 0xFF4B91A8, 0xFFE29A36
    };
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_SceneBegin(GFX_TOP);
    C2D_DrawRectSolid(0, 0, 0, 400, 240, 0, C2D_Color32(14, 17, 25, 255));
    C2D_DrawRectSolid(0, 0, 0.1f, 400, 35, 0, C2D_Color32(25, 30, 43, 255));
    launcherTextDraw("CINTENDO", 66, 10, 0.65f);
    launcherTextDraw("HOME", 350, 12, 0.45f);
    for (int i = 0; i < 9; i++)
    {
        float x = 18.0f + (float)(i % 3) * 128.0f;
        float y = 49.0f + (float)(i / 3) * 61.0f;
        u32 tile = colors[i];
        if (selected == i)
        {
            C2D_DrawRectSolid(x - 3, y - 3, 0.2f, 112, 55, 7, C2D_Color32(245, 245, 255, 255));
            C2D_DrawRectSolid(x, y, 0.3f, 106, 49, 6, tile);
        }
        else
            C2D_DrawRectSolid(x, y, 0.2f, 106, 49, 6, tile);
        launcherTextDraw(symbols[i], x + 53, y + 5, 0.9f);
        launcherTextDraw(labels[i], x + 53, y + 31, 0.36f);
    }
    C2D_DrawRectSolid(0, 232, 0.2f, 400, 8, 0, C2D_Color32(35, 42, 58, 255));
    C2D_SceneBegin(GFX_BOTTOM);
    C2D_DrawRectSolid(0, 0, 0, 320, 240, 0, C2D_Color32(14, 17, 25, 255));
    C2D_DrawRectSolid(12, 12, 0.1f, 296, 88, 8, C2D_Color32(25, 30, 43, 255));
    launcherTextDraw("APP LAUNCHER", 160, 25, 0.5f);
    launcherTextDraw(labels[selected], 160, 54, 0.8f);
    launcherTextDraw("Your 3DS. Your apps.", 160, 78, 0.38f);
    C2D_DrawRectSolid(12, 113, 0.1f, 296, 105, 8, C2D_Color32(25, 30, 43, 255));
    launcherTextDraw("D-PAD  Navigate apps", 160, 126, 0.43f);
    launcherTextDraw("A      Open selected app", 160, 148, 0.43f);
    launcherTextDraw("B      Back     START Exit", 160, 170, 0.43f);
    launcherTextDraw("Made by danielwav1", 160, 201, 0.38f);
    C3D_FrameEnd(0);
}

static void drawList(int appMode)
{
    consoleSelect(&top);
    consoleClear();
    header(appMode ? "HOMEBREW LIBRARY" : page == VAULT ? "SAVE VAULT" : "FILE MANAGER");
    printf("\n%.44s\n", path);
    printf("----------------------------------------------\n");

    int parent = !appMode && page != VAULT && strcmp(path, "sdmc:/") != 0;
    int visible = 0;
    if (parent)
    {
        printf("%s ..  [Parent folder]\n", selected == 0 ? ">" : " ");
        visible++;
    }

    for (int i = scroll; i < count && visible < VISIBLE_ROWS; i++)
    {
        int row = i + parent;
        char size[20];
        if (entries[i].directory)
            snprintf(size, sizeof(size), "<DIR>");
        else
            formatSize(entries[i].size, size, sizeof(size));
        const char *kind = entries[i].directory ? "[DIR]" : hasExtension(entries[i].name, ".3dsx") ? "[3DSX]" : "[FILE]";
        printf("%s %s %-23.23s %8s\n", selected == row ? ">" : " ",
               kind, entries[i].name, size);
        visible++;
    }
    if (!count)
        printf("\n  Nothing here yet.\n");

    consoleSelect(&bottom);
    consoleClear();
    header(appMode ? "APP LIBRARY" : page == VAULT ? "SAVE VAULT" : "FILE BROWSER");
    printf("  %d items\n", count);
    if (hints)
    {
        printf("\n  UP/DOWN  Navigate\n");
        printf("  A        Open / details\n");
        printf("  B        Back\n");
    }
    if (page == VAULT)
        printf("\n  Vault location:\n  sdmc:/3ds/CINTENDO/SaveVault\n");
    printf("\n%s\n", status);
    footer();
}

static void drawSettings(void)
{
    consoleSelect(&top);
    consoleClear();
    header("SETTINGS");
    printf("\n%s %sControl hints: %s%s\n", selected == 0 ? ">" : " ",
           themes[themeIndex].primary, hints ? "ON" : "OFF", reset);
    printf("%s Dark Mode: %s\n", selected == 1 ? ">" : " ", darkMode ? "ON" : "OFF");
    printf("%s Theme: %s%s\n", selected == 2 ? ">" : " ",
           themes[themeIndex].primary, themes[themeIndex].name);
    printf("\n  Select a row and press A.\n");
    consoleSelect(&bottom);
    consoleClear();
    header("PREFERENCES");
    printf("  Dark Mode is enabled by default.\n");
    printf("  Theme settings open Theme Studio.\n");
    printf("  Your choices persist after restart.\n");
    printf("\n  B: Home");
    footer();
}

static void drawThemes(void)
{
    consoleSelect(&top);
    consoleClear();
    header("THEME STUDIO");
    printf("\n");
    for (int i = 0; i < (int)(sizeof(themes) / sizeof(themes[0])); i++)
    {
        printf("%s %s%s%s\n", selected == i ? ">" : " ",
               themes[i].primary, themes[i].name, reset);
        printf("    %s%s\n", themes[i].accent, themes[i].tagline);
        if (i == 1)
            printf("    * . + * . + * . + * . +\n");
    }
    consoleSelect(&bottom);
    consoleClear();
    header("LIVE PRESETS");
    printf("  Standard       Red / Blue / Black / White\n");
    printf("  Aesthetic Pink Pink / Green / Mint\n");
    printf("                 with sprinkle pattern\n");
    printf("  CindraOS       Red / Black / White\n");
    printf("  Midnight       Purple / Cyan\n");
    printf("  Ocean          Aqua / Blue\n");
    printf("  Sunset         Amber / Pink\n");
    printf("\n  A: Apply and save\n  B: Back");
    footer();
}

static void drawSystem(void)
{
    struct statvfs info;
    unsigned long long total = 0;
    unsigned long long freeBytes = 0;
    if (statvfs("sdmc:/", &info) == 0)
    {
        total = (unsigned long long)info.f_blocks * info.f_frsize;
        freeBytes = (unsigned long long)info.f_bavail * info.f_frsize;
    }
    char totalText[24], freeText[24], usedText[24];
    formatSize(total, totalText, sizeof(totalText));
    formatSize(freeBytes, freeText, sizeof(freeText));
    formatSize(total >= freeBytes ? total - freeBytes : 0, usedText, sizeof(usedText));

    consoleSelect(&top);
    consoleClear();
    header("SYSTEM DASHBOARD");
    printf("\n  DEVICE\n  Nintendo 3DS family\n  CINTENDO 1.3\n");
    printf("\n  STORAGE\n  Total: %s\n  Used:  %s\n  Free:  %s\n",
           totalText, usedText, freeText);
    printf("\n  SD mount: sdmc:/\n");
    consoleSelect(&bottom);
    consoleClear();
    header("DIAGNOSTICS");
    printf("  SD card statistics via statvfs.\n");
    printf("  This app does not modify system\n  firmware or NAND settings.\n\n");
    printf("  B: Home");
    footer();
}

static void drawVault(void)
{
    consoleSelect(&top);
    consoleClear();
    header("SAVE VAULT");
    printf("\n  Personal save-backup archive\n\n");
    printf("  Put exported backup files in:\n");
    printf("  sdmc:/3ds/CINTENDO/SaveVault/\n\n");
    printf("  Open the folder to inspect files.\n");
    printf("  CINTENDO keeps these as ordinary\n");
    printf("  backup copies on your SD card.\n");
    printf("\n  No game save is overwritten.\n");
    consoleSelect(&bottom);
    consoleClear();
    header("SAVE DATA SAFETY");
    printf("  For game restore, use Checkpoint or\n");
    printf("  JKSM and choose the correct title.\n\n");
    printf("  A generic .sav import cannot safely\n");
    printf("  restore every 3DS game's save format.\n");
    printf("\n  A: Open Vault folder\n  B: Home");
    footer();
}

static void drawAbout(void)
{
    consoleSelect(&top);
    consoleClear();
    header("ABOUT CINTENDO");
    printf("\n  CINTENDO 1.3\n");
    printf("  A homebrew utility for Nintendo 3DS.\n\n");
    printf("  Made by danielwav1\n");
    printf("  github.com/danielwav1/CINTENDO\n\n");
    printf("  Built with devkitPro and libctru.\n");
    consoleSelect(&bottom);
    consoleClear();
    header("PROJECT");
    printf("  Independent community homebrew.\n");
    printf("  Not affiliated with Nintendo.\n\n");
    printf("  B: Home");
    footer();
}

static void drawDetails(void)
{
    struct stat info;
    consoleSelect(&top);
    consoleClear();
    header("FILE DETAILS");
    printf("\n  %.42s\n\n", strrchr(detail, '/') ? strrchr(detail, '/') + 1 : detail);
    printf("  Path: %.42s\n", detail);
    if (stat(detail, &info) == 0)
    {
        char size[24];
        formatSize((unsigned long long)info.st_size, size, sizeof(size));
        printf("  Type: %s\n", S_ISDIR(info.st_mode) ? "Directory" : "File");
        printf("  Size: %s\n", size);
    }
    consoleSelect(&bottom);
    consoleClear();
    header("FILE ACTIONS");
    printf("  X: Copy file into Save Vault\n");
    printf("  B: Back\n\n");
    printf("  Importing here creates an archive\n");
    printf("  copy. It does not restore a game.");
    footer();
}

static void drawAdvanced(void)
{
    consoleSelect(&top);
    consoleClear();
    header("ADVANCED TOOLS");
    printf("\n  [TOOLS] Plugin Center\n");
    printf("  [RESET] Reset CINTENDO settings\n\n");
    printf("  A: Open Plugin Center\n");
    printf("  Y: Reset CINTENDO\n");
    printf("  Privilege: standard homebrew only\n");
    printf("  NAND/root access: unavailable\n");
    consoleSelect(&bottom);
    consoleClear();
    header("ADVANCED MODE");
    printf("  Unlocked by L + R + X + Y.\n\n");
    printf("  This is an interface lock only.\n");
    printf("  It does not grant root privileges,\n");
    printf("  bypass Luma3DS or elevate permissions.\n");
    printf("\n  B: Home");
    footer();
}

static void draw(void)
{
    if (page == HOME) drawHome();
    else if (page == MUSIC) drawMusic();
    else if (page == FILES) drawList(0);
    else if (page == APPS) drawList(1);
    else if (page == VAULT) drawVault();
    else if (page == SYSTEM) drawSystem();
    else if (page == SETTINGS) drawSettings();
    else if (page == THEMES) drawThemes();
    else if (page == ABOUT) drawAbout();
    else if (page == DETAILS) drawDetails();
    else if (page == ADVANCED) drawAdvanced();
    else if (page == PLUGINS) drawPlugins();
    else if (page == PLUGIN_DETAILS) drawPluginDetails();
    else if (page == RESET_CONFIRM) { consoleSelect(&top); consoleClear(); header("RESET CINTENDO"); printf("\n  Reset settings and remove installed plugins?\n\n  Save Vault backups will be kept.\n"); consoleSelect(&bottom); consoleClear(); header("CONFIRMATION"); printf("  Y: Confirm reset\n  B: Cancel"); footer(); }
    if (page != HOME)
    {
        gfxFlushBuffers();
        gfxSwapBuffers();
    }
}

static void move(int direction, int total)
{
    if (total <= 0)
    {
        selected = 0;
        return;
    }
    selected = (selected + direction + total) % total;
    if (selected < scroll)
        scroll = selected;
    if (selected >= scroll + VISIBLE_ROWS)
        scroll = selected - VISIBLE_ROWS + 1;
}

static void parentFolder(void)
{
    char *slash = strrchr(path, '/');
    if (slash && slash > path + 5)
        *slash = '\0';
    else
        snprintf(path, sizeof(path), "sdmc:/");
}

static void openEntry(int appMode)
{
    int parent = !appMode && page == FILES && strcmp(path, "sdmc:/") != 0;
    if (parent && selected == 0)
    {
        parentFolder();
        scan(path, 0);
        dirty = 1;
        return;
    }

    int index = selected - parent;
    if (index < 0 || index >= count)
        return;

    char full[PATH_SIZE];
    int length = snprintf(full, sizeof(full), "%s%s%s", path,
                          path[strlen(path) - 1] == '/' ? "" : "/",
                          entries[index].name);
    if (length < 0 || length >= (int)sizeof(full))
    {
        statusSet("Path is too long.");
        dirty = 1;
        return;
    }

    if (!appMode && entries[index].directory)
    {
        snprintf(path, sizeof(path), "%s", full);
        scan(path, 0);
        dirty = 1;
        return;
    }

    snprintf(detail, sizeof(detail), "%s", full);
    previousPage = page;
    page = DETAILS;
    dirty = 1;
}

static int copyToVault(const char *source)
{
    struct stat info;
    if (stat(source, &info) != 0 || !S_ISREG(info.st_mode))
        return 0;

    ensureFolders();
    const char *name = strrchr(source, '/');
    name = name ? name + 1 : source;

    char destination[PATH_SIZE];
    if (snprintf(destination, sizeof(destination), "sdmc:/3ds/CINTENDO/SaveVault/%s", name) >= (int)sizeof(destination))
        return 0;

    if (strcmp(source, destination) == 0)
        return 0;

    struct stat existing;
    if (stat(destination, &existing) == 0)
        return 0;

    FILE *input = fopen(source, "rb");
    if (!input)
        return 0;

    FILE *output = fopen(destination, "wb");
    if (!output)
    {
        fclose(input);
        return 0;
    }

    unsigned char buffer[4096];
    size_t bytes;
    int ok = 1;
    while ((bytes = fread(buffer, 1, sizeof(buffer), input)) > 0)
    {
        if (fwrite(buffer, 1, bytes, output) != bytes)
        {
            ok = 0;
            break;
        }
    }
    if (ferror(input))
        ok = 0;
    fclose(input);
    if (fclose(output) != 0)
        ok = 0;
    return ok;
}

int main(void)
{
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    launcherText = C2D_TextBufNew(4096);
    ndspInit();
    consoleInit(GFX_TOP, &top);
    consoleInit(GFX_BOTTOM, &bottom);
    ensureFolders();
    settingsLoad();
    scanPlugins();

    while (aptMainLoop() && running)
    {
        hidScanInput();
        u32 keys = hidKeysDown();
        u32 held = hidKeysHeld();
        u32 combo = KEY_L | KEY_R | KEY_X | KEY_Y;

        if ((held & combo) == combo && !comboWasHeld)
        {
            advancedUnlocked = !advancedUnlocked;
            comboWasHeld = 1;
            statusSet(advancedUnlocked ? "Advanced tools unlocked." : "Advanced tools locked.");
            dirty = 1;
        }
        if ((held & combo) != combo)
            comboWasHeld = 0;

        if (keys & KEY_START)
            running = 0;

        if (page == HOME)
        {
            if (keys & KEY_LEFT) { move(-1, 9); dirty = 1; }
            if (keys & KEY_RIGHT) { move(1, 9); dirty = 1; }
            if (keys & KEY_UP) { move(-3, 9); dirty = 1; }
            if (keys & KEY_DOWN) { move(3, 9); dirty = 1; }
            if (keys & KEY_A)
            {
                if (selected == 0)
                {
                    snprintf(path, sizeof(path), "sdmc:/");
                    scan(path, 0);
                    page = FILES;
                }
                else if (selected == 1)
                {
                    snprintf(path, sizeof(path), "sdmc:/3ds");
                    scan(path, 1);
                    page = APPS;
                }
                else if (selected == 2)
                    page = VAULT;
                else if (selected == 3)
                    page = THEMES;
                else if (selected == 4)
                    page = SYSTEM;
                else if (selected == 5)
                {
                    selected = 0;
                    page = SETTINGS;
                }
                else if (selected == 6 && advancedUnlocked)
                    page = ADVANCED;
                else if (selected == 7)
                    page = ABOUT;
                else if (selected == 8)
                {
                    scanMusic();
                    page = MUSIC;
                }
                else
                    statusSet("Hold L + R + X + Y to unlock Advanced Tools.");

                if (page == THEMES) selected = themeIndex;
                else if (page != SETTINGS) selected = 0;
                dirty = 1;
            }
        }
        else if (page == FILES || page == APPS)
        {
            int parent = page == FILES && strcmp(path, "sdmc:/") != 0;
            if (keys & KEY_UP) { move(-1, count + parent); dirty = 1; }
            if (keys & KEY_DOWN) { move(1, count + parent); dirty = 1; }
            if (keys & KEY_A) openEntry(page == APPS);
            if (keys & KEY_B)
            {
                if (page == APPS || !strcmp(path, "sdmc:/"))
                    page = HOME;
                else
                {
                    parentFolder();
                    scan(path, 0);
                }
                selected = 0;
                scroll = 0;
                dirty = 1;
            }
        }
        else if (page == MUSIC)
        {
            if (keys & KEY_UP) { move(-1, musicCount); dirty = 1; }
            if (keys & KEY_DOWN) { move(1, musicCount); dirty = 1; }
            if ((keys & KEY_A) && musicCount > 0)
            {
                if (playMusicFile(musicFiles[selected])) statusSet("Playing WAV audio.");
                else statusSet("Unsupported WAV. Use PCM 8/16-bit mono/stereo.");
                dirty = 1;
            }
            if (keys & KEY_X)
            {
                if (musicPlaying) { musicPaused = !musicPaused; ndspChnSetPaused(0, musicPaused); }
                dirty = 1;
            }
            if (keys & KEY_Y) { stopMusic(); dirty = 1; }
            if (keys & KEY_B) { page = HOME; selected = 0; dirty = 1; }
        }
        else if (page == DETAILS)
        {
            if (keys & KEY_X)
            {
                if (copyToVault(detail))
                    statusSet("Backup copy imported into Save Vault.");
                else
                    statusSet("Import failed. Check file and SD space.");
                dirty = 1;
            }
            if (keys & KEY_B)
            {
                page = previousPage;
                dirty = 1;
            }
        }
        else if (page == SETTINGS)
        {
            if (keys & KEY_UP) { selected = (selected + 2) % 3; dirty = 1; }
            if (keys & KEY_DOWN) { selected = (selected + 1) % 3; dirty = 1; }
            if (keys & KEY_A)
            {
                if (selected == 0)
                    hints = !hints;
                else if (selected == 1)
                    darkMode = !darkMode;
                else
                {
                    page = THEMES;
                    selected = themeIndex;
                }
                settingsSave();
                dirty = 1;
            }
            if (keys & KEY_B) { page = HOME; selected = 0; dirty = 1; }
        }
        else if (page == THEMES)
        {
            if (keys & KEY_UP) { move(-1, 6); dirty = 1; }
            if (keys & KEY_DOWN) { move(1, 6); dirty = 1; }
            if (keys & KEY_A)
            {
                themeIndex = selected;
                settingsSave();
                statusSet("Theme applied and saved.");
                dirty = 1;
            }
            if (keys & KEY_B) { page = HOME; selected = 0; dirty = 1; }
        }
        else if (page == VAULT)
        {
            if (keys & KEY_A)
            {
                snprintf(path, sizeof(path), "sdmc:/3ds/CINTENDO/SaveVault");
                scan(path, 0);
                page = FILES;
                dirty = 1;
            }
            if (keys & KEY_B) { page = HOME; selected = 0; dirty = 1; }
        }
        else if (page == ADVANCED || page == SYSTEM || page == ABOUT)
        {
            if (keys & KEY_B) { page = HOME; selected = 0; dirty = 1; }
            if (page == ADVANCED && (keys & KEY_A))
            {
                page = PLUGINS;
                selected = 0;
                dirty = 1;
            }
            if (page == ADVANCED && (keys & KEY_Y))
            {
                page = RESET_CONFIRM;
                dirty = 1;
            }
        }
        else if (page == PLUGINS)
        {
            scanPlugins();
            if (keys & KEY_UP) { move(-1, pluginCount); dirty = 1; }
            if (keys & KEY_DOWN) { move(1, pluginCount); dirty = 1; }
            if ((keys & KEY_A) && pluginCount > 0 && selected < pluginCount)
            {
                if (pluginEngineToggle(&plugins[selected]))
                    statusSet(plugins[selected].enabled ? "Plugin key disabled." : "Plugin key enabled.");
                scanPlugins();
                dirty = 1;
            }
            if ((keys & KEY_X) && pluginCount > 0 && selected < pluginCount)
            {
                activePlugin = plugins[selected];
                snprintf(pluginResult, sizeof(pluginResult), "Press X to execute this script.");
                page = PLUGIN_DETAILS;
                dirty = 1;
            }
            if (keys & KEY_B) { page = ADVANCED; selected = 0; dirty = 1; }
        }
        else if (page == PLUGIN_DETAILS)
        {
            if (keys & KEY_A)
            {
                if (pluginEngineToggle(&activePlugin))
                {
                    activePlugin.enabled = !activePlugin.enabled;
                    snprintf(pluginResult, sizeof(pluginResult), "Plugin is now %s.", activePlugin.enabled ? "enabled" : "disabled");
                }
                else
                    snprintf(pluginResult, sizeof(pluginResult), "Could not change activation state.");
                dirty = 1;
            }
            if (keys & KEY_X)
            {
                if (pluginEngineRun(&activePlugin, &themeIndex, (int)(sizeof(themes) / sizeof(themes[0])), pluginResult, sizeof(pluginResult)))
                {
                    settingsSave();
                    statusSet("Plugin script executed.");
                }
                else
                    statusSet("Plugin disabled or script action unsupported.");
                dirty = 1;
            }
            if (keys & KEY_B) { page = PLUGINS; scanPlugins(); dirty = 1; }
        }
        else if (page == RESET_CONFIRM)
        {
            if (keys & KEY_Y)
            {
                resetCintendo();
                page = ADVANCED;
                dirty = 1;
            }
            if (keys & KEY_B) { page = ADVANCED; dirty = 1; }
        }

        if (dirty)
        {
            draw();
            dirty = 0;
        }
        gspWaitForVBlank();
    }

    stopMusic();
    ndspExit();
    C2D_TextBufDelete(launcherText);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
