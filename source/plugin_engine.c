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

#define MAX_PLUGIN_FILE_SIZE 2048

static int pluginFileAllowed(const char *path)
{
    struct stat info;
    return stat(path, &info) == 0 && S_ISREG(info.st_mode) &&
        info.st_size > 0 && info.st_size <= MAX_PLUGIN_FILE_SIZE;
}

static int readPluginLine(FILE *file, char *line, size_t lineSize)
{
    if (!fgets(line, (int)lineSize, file))
        return 0;
    size_t length = strlen(line);
    if (length == lineSize - 1 && line[length - 1] != '\n' && !feof(file))
        return -1;
    line[strcspn(line, "\r\n")] = 0;
    return 1;
}

static int copyPluginField(char *destination, size_t destinationSize, const char *value)
{
    size_t length = strlen(value);
    if (!destination || !value || length == 0 || length >= destinationSize)
        return 0;
    memcpy(destination, value, length + 1);
    return 1;
}

static int parseKey(const char *path, char *id, size_t idSize)
{
    if (!id || idSize < 2 || !pluginFileAllowed(path))
        return 0;
    FILE *file = fopen(path, "r");
    if (!file)
        return 0;
    char line[192];
    int header = 0;
    int readResult;
    id[0] = 0;
    while ((readResult = readPluginLine(file, line, sizeof(line))) > 0)
    {
        if (!strcmp(line, "CINTENDO_KEY_V1"))
            header = 1;
        else if (!strncmp(line, "id=", 3))
        {
            if (!copyPluginField(id, idSize, line + 3))
            {
                fclose(file);
                return 0;
            }
        }
    }
    fclose(file);
    return readResult == 0 && header && safeId(id);
}

static int parseScript(const char *path, CintendoPlugin *plugin)
{
    if (!plugin || !pluginFileAllowed(path))
        return 0;
    FILE *file = fopen(path, "r");
    if (!file)
        return 0;
    char line[256];
    int header = 0;
    int readResult;
    char scriptId[48] = "";
    memset(plugin, 0, sizeof(*plugin));
    while ((readResult = readPluginLine(file, line, sizeof(line))) > 0)
    {
        if (!strcmp(line, "CINTENDO_SCRIPT_V1"))
            header = 1;
        else if (!strncmp(line, "id=", 3))
        {
            if (!copyPluginField(scriptId, sizeof(scriptId), line + 3))
            {
                fclose(file);
                return 0;
            }
        }
        else if (!strncmp(line, "name=", 5))
        {
            if (!copyPluginField(plugin->name, sizeof(plugin->name), line + 5))
            {
                fclose(file);
                return 0;
            }
        }
        else if (!strncmp(line, "description=", 12))
        {
            if (!copyPluginField(plugin->description, sizeof(plugin->description), line + 12))
            {
                fclose(file);
                return 0;
            }
        }
        else if (!strncmp(line, "action=", 7))
        {
            if (!copyPluginField(plugin->action, sizeof(plugin->action), line + 7))
            {
                fclose(file);
                return 0;
            }
        }
    }
    fclose(file);
    if (readResult != 0 || !header || !safeId(scriptId) ||
        !plugin->name[0] || !plugin->description[0] || !plugin->action[0])
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
    if (!plugins || capacity <= 0)
        return 0;
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
