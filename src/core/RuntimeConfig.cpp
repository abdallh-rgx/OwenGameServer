#include "RuntimeConfig.hpp"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <android/log.h>

OwenConfig OwenCfg;

static bool Truthy(const char* v) {
    if (!v) return false;
    return strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0 ||
           strcasecmp(v, "yes") == 0 || strcasecmp(v, "on") == 0;
}

static char* TrimInPlace(char* s) {
    while (*s == ' ' || *s == '\t' || *s == '\r') s++;
    char* end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
        *--end = 0;
    return s;
}

bool LoadOwenConfig(const char* logDirOrNull) {
    const char* dirs[5];
    int ndirs = 0;
    if (logDirOrNull && *logDirOrNull) dirs[ndirs++] = logDirOrNull;
    dirs[ndirs++] = "/storage/emulated/0/Android/data/com.epicgames.fortnite/files";
    dirs[ndirs++] = "/sdcard/Android/data/com.epicgames.fortnite/files";
    dirs[ndirs++] = "/data/data/com.epicgames.fortnite/files";
    dirs[ndirs++] = "/data/local/tmp";

    for (int i = 0; i < ndirs; i++) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/OwenGameServer.cfg", dirs[i]);
        FILE* f = fopen(path, "r");
        if (!f) continue;

        __android_log_print(ANDROID_LOG_INFO, "OwenGameServer",
                            "[CFG] reading runtime config: %s", path);

        char line[512];
        int applied = 0;
        while (fgets(line, sizeof(line), f)) {
            char* s = TrimInPlace(line);
            if (!*s || *s == '#') continue;
            char* eq = strchr(s, '=');
            if (!eq) continue;
            *eq = 0;
            char* key = TrimInPlace(s);
            char* val = TrimInPlace(eq + 1);
            bool b = Truthy(val);

            bool known = true;
            if      (strcmp(key, "safe_mode") == 0)            OwenCfg.safe_mode = b;
            else if (strcmp(key, "no_setclientoffonly") == 0)  OwenCfg.no_setclientoffonly = b;
            else if (strcmp(key, "late_flip") == 0)            OwenCfg.late_flip = b;
            else if (strcmp(key, "no_getnetmode_hook") == 0)   OwenCfg.no_getnetmode_hook = b;
            else if (strcmp(key, "honest_netmode") == 0)       OwenCfg.honest_netmode = b;
            else if (strcmp(key, "no_tickflush_hook") == 0)    OwenCfg.no_tickflush_hook = b;
            else if (strcmp(key, "no_native_hooks") == 0)      OwenCfg.no_native_hooks = b;
            else if (strcmp(key, "no_exec_hooks") == 0)        OwenCfg.no_exec_hooks = b;
            else if (strcmp(key, "no_map_travel") == 0)        OwenCfg.no_map_travel = b;
            else if (strcmp(key, "no_fatal_api_hooks") == 0)   OwenCfg.no_fatal_api_hooks = b;
            else known = false;

            __android_log_print(ANDROID_LOG_INFO, "OwenGameServer",
                                "[CFG]   %s=%s%s", key, val, known ? "" : "  (UNKNOWN KEY - ignored)");
            if (known) applied++;
        }
        fclose(f);
        __android_log_print(ANDROID_LOG_INFO, "OwenGameServer",
                            "[CFG] %d option(s) applied from %s", applied, path);
        return true;
    }
    __android_log_print(ANDROID_LOG_INFO, "OwenGameServer",
                        "[CFG] no OwenGameServer.cfg found - running with defaults "
                        "(current behaviour, forensics ON)");
    return false;
}
