/*
 * PiFmRds-Enchanted - Configuration file parser
 * Modernized for 2026 standards
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "config_file.h"

void pifm_config_defaults(pifm_app_config_t *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(pifm_app_config_t));
    cfg->default_freq = 107.9;
    strncpy(cfg->default_ps, "ENCHNTED", sizeof(cfg->default_ps) - 1);
    strncpy(cfg->default_rt, "Radio PiFmRds-Enchanted 2026", sizeof(cfg->default_rt) - 1);
    cfg->default_pi = 0x1234;
    cfg->default_stereo = true;
    cfg->default_preemph = 50;
    cfg->default_gain = 1.0f;
    strncpy(cfg->default_pipe, "/tmp/pifm_ctl", sizeof(cfg->default_pipe) - 1);
    strncpy(cfg->default_sock, "/tmp/pifm_sock", sizeof(cfg->default_sock) - 1);
}

static char *trim_whitespace(char *str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

bool pifm_config_load(pifm_app_config_t *cfg, const char *custom_path) {
    pifm_config_defaults(cfg);

    char filepath[512] = {0};
    FILE *fp = NULL;

    if (custom_path) {
        strncpy(filepath, custom_path, sizeof(filepath) - 1);
        fp = fopen(filepath, "r");
    } else {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(filepath, sizeof(filepath), "%s/.config/pifm/pifm.conf", home);
            fp = fopen(filepath, "r");
        }
        if (!fp) {
            strncpy(filepath, "/etc/pifm.conf", sizeof(filepath) - 1);
            fp = fopen(filepath, "r");
        }
    }

    if (!fp) return false;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char *p = trim_whitespace(line);
        if (*p == '#' || *p == ';' || *p == '\0') continue;

        char *eq = strchr(p, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = trim_whitespace(p);
        char *val = trim_whitespace(eq + 1);

        /* Remove surrounding quotes if present */
        size_t vlen = strlen(val);
        if (vlen >= 2 && ((val[0] == '"' && val[vlen - 1] == '"') || (val[0] == '\'' && val[vlen - 1] == '\''))) {
            val[vlen - 1] = '\0';
            val++;
        }

        if (strcasecmp(key, "freq") == 0 || strcasecmp(key, "frequency") == 0) {
            cfg->default_freq = atof(val);
        } else if (strcasecmp(key, "ps") == 0 || strcasecmp(key, "station") == 0) {
            strncpy(cfg->default_ps, val, sizeof(cfg->default_ps) - 1);
        } else if (strcasecmp(key, "rt") == 0 || strcasecmp(key, "radiotext") == 0) {
            strncpy(cfg->default_rt, val, sizeof(cfg->default_rt) - 1);
        } else if (strcasecmp(key, "pi") == 0) {
            cfg->default_pi = (uint16_t)strtol(val, NULL, 16);
        } else if (strcasecmp(key, "stereo") == 0) {
            cfg->default_stereo = (strcasecmp(val, "true") == 0 || strcmp(val, "1") == 0 || strcasecmp(val, "yes") == 0);
        } else if (strcasecmp(key, "preemph") == 0) {
            cfg->default_preemph = atoi(val);
        } else if (strcasecmp(key, "gain") == 0) {
            cfg->default_gain = (float)atof(val);
        }
    }

    fclose(fp);
    return true;
}
