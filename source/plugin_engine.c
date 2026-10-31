#include "plugin_engine.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/stat.h>

#define KEY_DIR "sdmc:/3ds/CINTENDO/PluginKeys"
#define SCRIPT_DIR "sdmc:/3ds/CINTENDO/Plugins"

static int extensionIs(const char *name, const char *extension)
{
    size_t a = strlen(name);
    size_t b = strlen(extension);
    return a >= b && strcasecmp(name + a - b, extension) == 0;
}

static int safeId(const char *id)
{
    if (!id[0] || strlen(id) > 40)
        return 0;
    for (const char *p = id; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-'))
            return 0;
    return 1;
}

static int parseKey(const char *path, char *id, size_t idSize)
{
    FILE *file = fopen(path, "r");
    if (!file)
        return 0;
    char line[192];
    int header = 0;
    id[0] = 0;
    while (fgets(line, sizeof(line), file))
    {
        line[strcspn(line, "\r\n")] = 0;
        if (!strcmp(line, "CINTENDO_KEY_V1"))
            header = 1;
        else if (!strncmp(line, "id=", 3))
            snprintf(id, idSize, "%s", line + 3);
    }
    fclose(file);
    return header && safeId(id);
}

static int parseScript(const char *path, CintendoPlugin *plugin)
{
    FILE *file = fopen(path, "r");
    if (!file)
        return 0;
    char line[256];
    int header = 0;
    char scriptId[48] = "";
    memset(plugin, 0, sizeof(*plugin));
    while (fgets(line, sizeof(line), file))
    {
        line[strcspn(line, "\r\n")] = 0;
        if (!strcmp(line, "CINTENDO_SCRIPT_V1"))
            header = 1;
        else if (!strncmp(line, "id=", 3))
            snprintf(scriptId, sizeof(scriptId), "%s", line + 3);
        else if (!strncmp(line, "name=", 5))
            snprintf(plugin->name, sizeof(plugin->name), "%s", line + 5);
        else if (!strncmp(line, "description=", 12))
            snprintf(plugin->description, sizeof(plugin->description), "%s", line + 12);
        else if (!strncmp(line, "action=", 7))
            snprintf(plugin->action, sizeof(plugin->action), "%s", line + 7);
    }
    fclose(file);
    if (!header || !safeId(scriptId) || !plugin->name[0] || !plugin->description[0] || !plugin->action[0])
        return 0;
    snprintf(plugin->id, sizeof(plugin->id), "%s", scriptId);
    return 1;
}

static int isEnabled(const char *id)
{
    char path[256];
    struct stat info;
    snprintf(path, sizeof(path), KEY_DIR "/%s.active", id);
    return stat(path, &info) == 0;
}

int pluginEngineScan(CintendoPlugin *plugins, int capacity)
{
    DIR *dir = opendir(KEY_DIR);
    int count = 0;
    if (!dir)
        return 0;
    struct dirent *item;
    while ((item = readdir(dir)) && count < capacity)
    {
        if (!extensionIs(item->d_name, ".cplug"))
            continue;
        char keyPath[256];
        char id[48];
        if (snprintf(keyPath, sizeof(keyPath), KEY_DIR "/%s", item->d_name) >= (int)sizeof(keyPath))
            continue;
        if (!parseKey(keyPath, id, sizeof(id)))
            continue;
        char scriptPath[256];
        if (snprintf(scriptPath, sizeof(scriptPath), SCRIPT_DIR "/%s.cscript", id) >= (int)sizeof(scriptPath))
            continue;
        CintendoPlugin candidate;
        if (!parseScript(scriptPath, &candidate) || strcmp(candidate.id, id))
            continue;
        candidate.enabled = isEnabled(id);
        plugins[count++] = candidate;
    }
    closedir(dir);
    return count;
}

int pluginEngineToggle(const CintendoPlugin *plugin)
{
    char path[256];
    struct stat info;
    if (!plugin || !safeId(plugin->id))
        return 0;
    snprintf(path, sizeof(path), KEY_DIR "/%s.active", plugin->id);
    if (stat(path, &info) == 0)
        return remove(path) == 0;
    FILE *file = fopen(path, "w");
    if (!file)
        return 0;
    fputs("enabled=1\n", file);
    return fclose(file) == 0;
}

int pluginEngineRun(const CintendoPlugin *plugin, int *themeIndex, int themeCount, char *message, unsigned int messageSize)
{
    if (!plugin || !plugin->enabled || !themeIndex || themeCount < 1 || !message || messageSize < 2)
        return 0;
    if (!strcmp(plugin->action, "THEME_NEXT"))
    {
        *themeIndex = (*themeIndex + 1) % themeCount;
        snprintf(message, messageSize, "Theme changed to preset %d.", *themeIndex + 1);
        return 1;
    }
    if (!strcmp(plugin->action, "THEME_PREVIOUS"))
    {
        *themeIndex = (*themeIndex + themeCount - 1) % themeCount;
        snprintf(message, messageSize, "Theme changed to preset %d.", *themeIndex + 1);
        return 1;
    }
    if (!strcmp(plugin->action, "THEME_RANDOM"))
    {
        srand((unsigned int)time(NULL) ^ (unsigned int)clock());
        *themeIndex = rand() % themeCount;
        snprintf(message, messageSize, "Random theme selected: preset %d.", *themeIndex + 1);
        return 1;
    }
    if (!strcmp(plugin->action, "SHOW_ABOUT"))
    {
        snprintf(message, messageSize, "CINTENDO plugin script engine v1. Made by danielwav1.");
        return 1;
    }
    snprintf(message, messageSize, "Unknown or unsupported action: %.32s", plugin->action);
    return 0;
}
