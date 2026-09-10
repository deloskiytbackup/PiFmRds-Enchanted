/*
 * PiFmRds-Enchanted - Configuration file parser
 * Modernized for 2026 standards
 * Loads defaults from /etc/pifm.conf or ~/.config/pifm/pifm.conf
 */

#ifndef CONFIG_FILE_H
#define CONFIG_FILE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double default_freq;
    char default_ps[32];
    char default_rt[128];
    uint16_t default_pi;
    bool default_stereo;
    int default_preemph;
    float default_gain;
    char default_pipe[128];
    char default_sock[128];
} pifm_app_config_t;

/* Load config from standard locations or specified path */
bool pifm_config_load(pifm_app_config_t *cfg, const char *custom_path);

/* Get default configuration values */
void pifm_config_defaults(pifm_app_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_FILE_H */
