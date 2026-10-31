#ifndef CINTENDO_PLUGIN_ENGINE_H
#define CINTENDO_PLUGIN_ENGINE_H

#define CINTENDO_PLUGIN_LIMIT 32

typedef struct
{
    char id[48];
    char name[96];
    char description[160];
    char action[48];
    int enabled;
} CintendoPlugin;

int pluginEngineScan(CintendoPlugin *plugins, int capacity);
int pluginEngineToggle(const CintendoPlugin *plugin);
int pluginEngineRun(const CintendoPlugin *plugin, int *themeIndex, int themeCount, char *message, unsigned int messageSize);

#endif
