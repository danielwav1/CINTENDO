#include <3ds.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

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
    RESET_CONFIRM
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
static Entry entries[MAX_ENTRIES];
static int count;
static int selected;
static int scroll;
static int themeIndex;
static int hints = 1;
static int advancedUnlocked;
static int running = 1;
static int dirty = 1;
static int comboWasHeld;
static Page page = HOME;
static Page previousPage = FILES;
static char path[PATH_SIZE] = "sdmc:/";
static char detail[PATH_SIZE];
static char status[100] = "Welcome to CINTENDO.";
typedef struct
{
    char filename[256];
    char id[48];
    char name[96];
    char description[160];
} Plugin;
static Plugin plugins[32];
static int pluginCount;
static Plugin activePlugin;

static const char *homeLabels[] = {
    "[FM]  File Manager",
    "[HB]  Homebrew Library",
    "[SV]  Save Vault",
    "[TH]  Theme Studio",
    "[SYS] System Dashboard",
    "[SET] Settings",
    "[ADV] Advanced Tools",
    "[i]   About CINTENDO"
};


static void statusSet(const char *value);
static void ensureFolders(void);
static int hasExtension(const char *name, const char *extension);

static int validPluginId(const char *id)
{
    static const char *allowed[] = {"battery","clock","storage","vault","filetypes","config","theme","folders","sdcheck","about"};
    for (int i = 0;i < 10;i++)
        if (strcmp(id, allowed[i]) == 0)
            return 1;
    return 0;
}

static int loadPluginFile(const char *full, Plugin *plugin)
{
    FILE *file = fopen(full, "r");
    if (!file)
        return 0;
    char line[256];
    int header = 0;
    memset(plugin, 0, sizeof(*plugin));
    while (fgets(line, sizeof(line), file))
    {
        line[strcspn(line, "\r\n")] = 0;
        if (strcmp(line, "CINTENDO_PLUGIN_V1") == 0)
            header = 1;
        else if (strncmp(line, "id=", 3) == 0)
            snprintf(plugin->id, sizeof(plugin->id), "%s", line + 3);
        else if (strncmp(line, "name=", 5) == 0)
            snprintf(plugin->name, sizeof(plugin->name), "%s", line + 5);
        else if (strncmp(line, "description=", 12) == 0)
            snprintf(plugin->description, sizeof(plugin->description), "%s", line + 12);
    }
    fclose(file);
    return header && validPluginId(plugin->id) && plugin->name[0] && plugin->description[0];
}

static void scanPlugins(void)
{
    DIR *dir = opendir("sdmc:/3ds/CINTENDO/Plugins");
    pluginCount = 0;
    if (!dir)
        return;
    struct dirent *item;
    while ((item = readdir(dir)) && pluginCount < 32)
    {
        if (!hasExtension(item->d_name, ".cplug"))
            continue;
        char full[PATH_SIZE];
        if (snprintf(full, sizeof(full), "sdmc:/3ds/CINTENDO/Plugins/%s", item->d_name) >= (int)sizeof(full))
            continue;
        Plugin candidate;
        if (loadPluginFile(full, &candidate))
        {
            snprintf(candidate.filename, sizeof(candidate.filename), "%s", item->d_name);
            plugins[pluginCount++] = candidate;
        }
    }
    closedir(dir);
}

static int installPluginsFromSD(void)
{
    DIR *dir = opendir("sdmc:/3ds/CINTENDO/PluginInstall");
    if (!dir)
        return 0;
    int installed = 0;
    struct dirent *item;
    while ((item = readdir(dir)))
    {
        if (!hasExtension(item->d_name, ".cplug"))
            continue;
        char source[PATH_SIZE], destination[PATH_SIZE];
        if (snprintf(source, sizeof(source), "sdmc:/3ds/CINTENDO/PluginInstall/%s", item->d_name) >= (int)sizeof(source) ||
            snprintf(destination, sizeof(destination), "sdmc:/3ds/CINTENDO/Plugins/%s", item->d_name) >= (int)sizeof(destination))
            continue;
        Plugin candidate;
        if (!loadPluginFile(source, &candidate))
            continue;
        FILE *in = fopen(source, "rb");
        FILE *out = fopen(destination, "wb");
        if (!in || !out)
        {
            if (in) fclose(in);
            if (out) fclose(out);
            continue;
        }
        char buffer[512];
        size_t n;
        int ok = 1;
        while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0)
            if (fwrite(buffer, 1, n, out) != n) { ok = 0; break; }
        if (ferror(in)) ok = 0;
        fclose(in);
        if (fclose(out) != 0) ok = 0;
        if (ok) installed++;
        else remove(destination);
    }
    closedir(dir);
    scanPlugins();
    return installed;
}

static void resetCintendo(void)
{
    remove("sdmc:/3ds/CINTENDO/config.ini");
    DIR *dir = opendir("sdmc:/3ds/CINTENDO/Plugins");
    if (dir)
    {
        struct dirent *item;
        while ((item = readdir(dir)))
        {
            if (!hasExtension(item->d_name, ".cplug"))
                continue;
            char full[PATH_SIZE];
            if (snprintf(full, sizeof(full), "sdmc:/3ds/CINTENDO/Plugins/%s", item->d_name) < (int)sizeof(full))
                remove(full);
        }
        closedir(dir);
    }
    themeIndex = 0;
    hints = 1;
    advancedUnlocked = 0;
    selected = 0;
    scroll = 0;
    scanPlugins();
    statusSet("CINTENDO reset. Save Vault backups were preserved.");
    settingsSave();
}

static void drawPlugins(void)
{
    scanPlugins();
    consoleSelect(&top);
    consoleClear();
    header("PLUGIN CENTER");
    printf("\n  Installed plugins: %d / 32\n\n", pluginCount);
    for (int i = 0; i < pluginCount && i < 14; i++)
        printf("  %s %s\n", selected == i ? ">" : " ", plugins[i].name);
    if (!pluginCount)
        printf("  No plugins installed yet.\n");
    consoleSelect(&bottom);
    consoleClear();
    header("SD CARD INSTALL");
    printf("  Copy .cplug packages into:\n");
    printf("  sdmc:/3ds/CINTENDO/PluginInstall/\n\n");
    printf("  Then press Y to install them.\n");
    printf("  A: Open selected plugin\n  Y: Install from SD\n  B: Advanced Tools");
    footer();
}

static void drawPluginDetails(void)
{
    consoleSelect(&top);
    consoleClear();
    header("PLUGIN DETAILS");
    printf("\n  %s\n\n", activePlugin.name);
    printf("  ID: %s\n", activePlugin.id);
    printf("  %s\n", activePlugin.description);
    consoleSelect(&bottom);
    consoleClear();
    header("PLUGIN RUNNER");
    if (strcmp(activePlugin.id, "battery") == 0)
        { u8 level = 0; PTMU_GetBatteryLevel(&level); printf("  Battery level: %u%%\n", (unsigned)level); }
    else if (strcmp(activePlugin.id, "clock") == 0)
        printf("  System tick: %llu\n", (unsigned long long)svcGetSystemTick());
    else if (strcmp(activePlugin.id, "storage") == 0)
    {
        struct statvfs v;
        if (statvfs("sdmc:/", &v) == 0)
            printf("  Free SD: %llu MB\n", (unsigned long long)v.f_bavail * v.f_frsize / 1048576ULL);
    }
    else if (strcmp(activePlugin.id, "vault") == 0)
    {
        DIR *d = opendir("sdmc:/3ds/CINTENDO/SaveVault");
        int n = 0;
        if (d) { struct dirent *e; while ((e = readdir(d))) if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) n++; closedir(d); }
        printf("  Backup files: %d\n", n);
    }
    else if (strcmp(activePlugin.id, "filetypes") == 0)
        printf("  Supported browser types: 3DSX, CIA, SAV, TXT, PNG, JPG, ZIP and more.\n");
    else if (strcmp(activePlugin.id, "config") == 0)
        printf("  Config: sdmc:/3ds/CINTENDO/config.ini\n  Theme index: %d\n  Hints: %s\n", themeIndex, hints ? "ON" : "OFF");
    else if (strcmp(activePlugin.id, "theme") == 0)
        printf("  Active theme: %s\n  Theme presets: %d\n", themes[themeIndex].name, (int)(sizeof(themes) / sizeof(themes[0])));
    else if (strcmp(activePlugin.id, "folders") == 0)
    {
        DIR *d = opendir("sdmc:/3ds");
        int n = 0;
        if (d) { struct dirent *e; while ((e = readdir(d))) if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) n++; closedir(d); }
        printf("  Entries in sdmc:/3ds: %d\n", n);
    }
    else if (strcmp(activePlugin.id, "sdcheck") == 0)
    {
        struct stat st;
        printf("  CINTENDO folder: %s\n", stat("sdmc:/3ds/CINTENDO", &st) == 0 ? "available" : "missing");
        printf("  Plugin folder: %s\n", stat("sdmc:/3ds/CINTENDO/Plugins", &st) == 0 ? "available" : "missing");
    }
    else if (strcmp(activePlugin.id, "about") == 0)
        printf("  CINTENDO plugin API v1\n  Made by danielwav1\n");
    printf("\n  B: Back");
    footer();
}
\nstatic void statusSet(const char *value);
static void ensureFolders(void);
static int hasExtension(const char *name, const char *extension);

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
    mkdir("sdmc:/3ds/CINTENDO/PluginInstall", 0777);
}

static void settingsLoad(void)
{
    FILE *file = fopen("sdmc:/3ds/CINTENDO/config.ini", "r");
    if (!file)
        return;

    int savedTheme = 0;
    int savedHints = 1;
    if (fscanf(file, "%d %d", &savedTheme, &savedHints) == 2)
    {
        if (savedTheme >= 0 && savedTheme < (int)(sizeof(themes) / sizeof(themes[0])))
            themeIndex = savedTheme;
        hints = savedHints ? 1 : 0;
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
    fprintf(file, "%d %d\n", themeIndex, hints);
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
    printf("%sCINTENDO%s  %s%s\n", themes[themeIndex].primary, reset,
           themes[themeIndex].secondary, section);
    printf("%s==============================================%s\n", themes[themeIndex].accent, reset);
}

static void footer(void)
{
    printf("\n%sMade by danielwav1%s", themes[themeIndex].primary, reset);
}

static void drawHome(void)
{
    consoleSelect(&top);
    consoleClear();
    header("HOME");
    printf("\n");
    printf("%sYour 3DS. Your way.%s\n", themes[themeIndex].accent, reset);
    printf("%s%s%s\n\n", themes[themeIndex].secondary, themes[themeIndex].tagline, reset);
    for (int i = 0; i < 8; i++)
        printf("%s %s%s%s\n", selected == i ? ">" : " ",
               selected == i ? themes[themeIndex].primary : reset,
               homeLabels[i], reset);
    footer();

    consoleSelect(&bottom);
    consoleClear();
    header("QUICK PANEL");
    printf("  [FM] Browse SD card\n");
    printf("  [HB] Find installed homebrew\n");
    printf("  [SV] Archive save backups\n");
    printf("  [TH] Change the whole look\n");
    printf("  [SYS] Storage dashboard\n\n");
    printf("%sSTATUS%s\n%s\n", themes[themeIndex].accent, reset, status);
    if (advancedUnlocked)
        printf("\n%sADVANCED MODE UNLOCKED%s", themes[themeIndex].primary, reset);
    else
        printf("\nHold L + R + X + Y for Advanced Tools");
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
    printf("%s Theme: %s%s\n", selected == 1 ? ">" : " ",
           themes[themeIndex].primary, themes[themeIndex].name);
    printf("\n  Select a row and press A.\n");
    consoleSelect(&bottom);
    consoleClear();
    header("PREFERENCES");
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
    printf("\n  DEVICE\n  Nintendo 3DS family\n  CINTENDO 1.1\n");
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
    printf("\n  CINTENDO 1.1\n");
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
    gfxFlushBuffers();
    gfxSwapBuffers();
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
    consoleInit(GFX_TOP, &top);
    consoleInit(GFX_BOTTOM, &bottom);
    ensureFolders();
    settingsLoad();\n    scanPlugins();

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
            if (keys & KEY_UP) { move(-1, 8); dirty = 1; }
            if (keys & KEY_DOWN) { move(1, 8); dirty = 1; }
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
            if (keys & KEY_UP) { selected = (selected + 1) % 2; dirty = 1; }
            if (keys & KEY_DOWN) { selected = (selected + 1) % 2; dirty = 1; }
            if (keys & KEY_A)
            {
                if (selected == 0)
                    hints = !hints;
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
                activePlugin = plugins[selected];
                page = PLUGIN_DETAILS;
                dirty = 1;
            }
            if (keys & KEY_Y)
            {
                int n = installPluginsFromSD();
                snprintf(status, sizeof(status), "%d plugin package(s) installed.", n);
                selected = 0;
                dirty = 1;
            }
            if (keys & KEY_B) { page = ADVANCED; selected = 0; dirty = 1; }
        }
        else if (page == PLUGIN_DETAILS)
        {
            if (keys & KEY_B) { page = PLUGINS; dirty = 1; }
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

    gfxExit();
    return 0;
}
