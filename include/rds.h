/*
 * PiFmRds-ng - Next-Generation FM/RDS transmitter
 * Modernized for 2026 standards
 *
 * RDS Engine Header (IEC 62106 / RBDS)
 */

#ifndef RDS_H
#define RDS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RDS_PS_LEN 8
#define RDS_RT_LEN 64
#define RDS_MAX_AF 25
#define RDS_BLOCK_COUNT 4

/* PTY standard type */
typedef enum {
    RDS_PTY_STANDARD_RDS = 0, /* European RDS standard */
    RDS_PTY_STANDARD_RBDS = 1  /* North American RBDS standard */
} rds_pty_standard_t;

/* Dynamic PS display modes */
typedef enum {
    RDS_PS_MODE_STATIC = 0,
    RDS_PS_MODE_PAGE = 1,    /* Paged 8-char words */
    RDS_PS_MODE_SCROLL = 2   /* Continuous scrolling */
} rds_ps_mode_t;

/* RT+ Content Type */
typedef enum {
    RT_PLUS_ITEM_TITLE = 1,
    RT_PLUS_ITEM_ALBUM = 2,
    RT_PLUS_ITEM_TRACK_NUMBER = 3,
    RT_PLUS_ITEM_ARTIST = 4,
    RT_PLUS_ITEM_COMPOSITION = 5,
    RT_PLUS_ITEM_MOVEMENT = 6,
    RT_PLUS_ITEM_CONDUCTOR = 7,
    RT_PLUS_ITEM_COMPOSER = 8,
    RT_PLUS_INFO_NEWS = 9,
    RT_PLUS_INFO_NEWS_LOCAL = 10,
    RT_PLUS_INFO_STOCKMARKET = 11,
    RT_PLUS_INFO_SPORT = 12,
    RT_PLUS_INFO_LOTTERY = 13,
    RT_PLUS_INFO_HOROSCOPE = 14,
    RT_PLUS_INFO_DAILY_DIVERSION = 15,
    RT_PLUS_INFO_HEALTH = 16,
    RT_PLUS_INFO_EVENT = 17,
    RT_PLUS_INFO_SCENE = 18,
    RT_PLUS_INFO_CINEMA = 19,
    RT_PLUS_INFO_TV = 20,
    RT_PLUS_INFO_DATE_TIME = 21,
    RT_PLUS_INFO_WEATHER = 22,
    RT_PLUS_INFO_TRAFFIC = 23,
    RT_PLUS_INFO_ALARM = 24,
    RT_PLUS_INFO_ADVERTISEMENT = 25,
    RT_PLUS_INFO_URL = 26,
    RT_PLUS_INFO_OTHER = 27,
    RT_PLUS_PROGRAMME_STATIONNAME_SHORT = 31,
    RT_PLUS_PROGRAMME_STATIONNAME_LONG = 32,
    RT_PLUS_PROGRAMME_PROGRAMME_NOW = 33,
    RT_PLUS_PROGRAMME_PROGRAMME_NEXT = 34,
    RT_PLUS_PROGRAMME_PART_NAME = 35,
    RT_PLUS_PROGRAMME_HOST = 36,
    RT_PLUS_PROGRAMME_EDITORIAL_STAFF = 37,
    RT_PLUS_PHONE_HOTLINE = 39,
    RT_PLUS_PHONE_STUDIO = 40,
    RT_PLUS_SMS_STUDIO = 44,
    RT_PLUS_EMAIL_HOTLINE = 45,
    RT_PLUS_EMAIL_STUDIO = 46,
    RT_PLUS_WEB_HOTLINE = 47,
    RT_PLUS_WEB_STUDIO = 48
} rt_plus_content_type_t;

/* RDS Configuration structure */
typedef struct {
    uint16_t pi;                  /* Program Identification (e.g. 0x3201 or 0x1234) */
    char ps[RDS_PS_LEN + 1];      /* Current 8-character Program Service */
    char dynamic_ps_text[256];    /* Full text for dynamic PS */
    rds_ps_mode_t ps_mode;        /* Static, Paging, or Scrolling */
    uint32_t ps_interval_ms;      /* Interval for PS updates */
    char rt[RDS_RT_LEN + 1];      /* RadioText (up to 64 chars) */
    bool rt_ab_flag;              /* A/B text toggle flag */
    bool ta;                      /* Traffic Announcement flag */
    bool tp;                      /* Traffic Programme flag */
    bool music;                   /* Music (1) / Speech (0) flag */
    uint8_t pty;                  /* Program Type (0-31) */
    rds_pty_standard_t pty_std;   /* RDS vs RBDS */
    bool enable_ct;               /* Clock Time (Group 4A) enable */
    
    /* RT+ (RadioText Plus) fields */
    bool rt_plus_enabled;
    char rt_plus_title[64];
    char rt_plus_artist[64];
    
    /* Alternative Frequencies (AF) in kHz (e.g. 87500 to 108000) */
    uint32_t af_list[RDS_MAX_AF];
    size_t af_count;
} rds_config_t;

/* Initialize RDS subsystem with defaults */
void rds_init(void);

/* Clean up RDS subsystem */
void rds_cleanup(void);

/* Parameter setters */
void set_rds_pi(uint16_t pi);
void set_rds_ps(const char *ps);
void set_rds_dynamic_ps(const char *text, rds_ps_mode_t mode, uint32_t interval_ms);
void set_rds_rt(const char *rt);
void set_rds_rt_plus(const char *title, const char *artist);
void set_rds_ta(bool ta);
void set_rds_tp(bool tp);
void set_rds_pty(uint8_t pty, rds_pty_standard_t standard);
void set_rds_music(bool music);
void set_rds_ct(bool enable);
void set_rds_af(const uint32_t *af_khz, size_t count);

/* Return pointer to current config (read-only) */
const rds_config_t *rds_get_config(void);

/* Fill a float buffer with RDS baseband samples (57 kHz subcarrier, BPSK shaped) */
void get_rds_samples(float *buffer, size_t count);

/* Low-level RDS CRC calculation for a 16-bit block */
uint16_t rds_calc_crc(uint16_t block);

/* Low-level RDS group builder (produces 4 blocks with offset words applied) */
void rds_build_group(uint16_t *blocks);

#ifdef __cplusplus
}
#endif

#endif /* RDS_H */
