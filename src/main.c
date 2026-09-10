/*
 * PiFmRds-Enchanted - FM/RDS Transmitter (2026 Edition)
 *
 * Modern, Intuitive Command Line Interface with Subcommands & Auto-detection
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <locale.h>
#include <math.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "rds.h"
#include "fm_mpx.h"
#include "hw_rpi.h"
#include "control.h"
#include "wav_io.h"
#include "iq_modulator.h"
#include "config_file.h"

#define PIFMRDS_VERSION "2.1.0-enchanted-2026"
#define BLOCK_SIZE 2280 /* 10 ms chunks at 228 kHz */

static volatile sig_atomic_t g_running = 1;
static wav_file_t *g_wav_out = NULL;
static iq_modulator_t g_iq_mod;
static bool g_is_iq_mode = false;

static void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
}

static void print_version(void) {
    printf("============================================================\n");
    printf("📻 PiFmRds-Enchanted (Wersja 2026)\n");
    printf("   Wersja:        %s\n", PIFMRDS_VERSION);
    printf("   Kompilacja:    %s %s\n", __DATE__, __TIME__);
#if defined(__clang__)
    printf("   Kompilator:    Clang %s\n", __clang_version__);
#elif defined(__GNUC__)
    printf("   Kompilator:    GCC %s\n", __VERSION__);
#endif
    printf("   Architektura:  %lu-bit\n", (unsigned long)(sizeof(void *) * 8));
    printf("------------------------------------------------------------\n");
    printf("Silnik Audio i Modulacji:\n");
    printf("   [✓] Formaty Hi-Fi:     24-bit PCM, 32-bit int/float, 16-bit PCM, 8-bit\n");
    printf("   [✓] Resampling:        4-punktowy Splajn Kubiczny Catmull-Rom (48k/96k/192k)\n");
    printf("   [✓] Auto-dekoder:      FLAC, MP3, AAC, M4A, OGG, Opus (w locie bez konwersji)\n");
    printf("   [✓] Broadcast DSP:     15 kHz Butterworth Brickwall LPF + AGC Soft Limiter\n");
    printf("   [✓] RDS / RBDS:        Grupy 0A (PS/AF), 2A (RT), 3A (ODA), 11A (RT+ Title/Artist)\n");
    printf("                          Grupa 4A (CT zegar MJD), Dynamic PS (Paging & Smooth Scroll)\n");
    printf("   [✓] SDR I/Q Mode:      HackRF, LimeSDR, FL2k, Raspberry Pi 5, PC/Mac\n");
    printf("   [✓] Sterowanie IPC:    Nieblokujący UNIX Domain Datagram Socket + FIFO\n");
    printf("------------------------------------------------------------\n");
    printf("Autorzy pierwotni (2012): Christophe Jacquet, Richard Hirst, Oliver Mattos\n");
    printf("Edycja Enchanted (2026):  Marcel Siepielski & Współtwórcy\n");
    printf("GitHub: https://github.com/deloskiytbackup/PiFmRds-Enchanted\n");
    printf("============================================================\n");
}

static void print_usage(const char *prog_name) {
    print_version();
    printf("\n🔥 Intuitive Usage (2026 Quick Syntax):\n");
    printf("  %s <audio_file> [freq_mhz] [options]\n", prog_name);
    printf("  %s 107.9 sound.wav\n", prog_name);
    printf("  %s play sound.wav 107.9 --station 'MYRADIO'\n", prog_name);
    printf("  %s ver\n", prog_name);
    printf("  %s ctl [ps|rt|title|artist|ta|stop] <value>\n\n", prog_name);

    printf("Commands & Subcommands:\n");
    printf("  ver, version            Wyświetl szczegółowe informacje o wersji i silniku\n");
    printf("  play <file> [mhz]       Broadcast an audio file directly\n");
    printf("  ctl <subcommand>        Send real-time commands to running transmission:\n");
    printf("                            ctl ps 'STATION'       Change 8-char station name\n");
    printf("                            ctl dynamic 'TEXT'     Set dynamic scrolling/paged PS\n");
    printf("                            ctl rt 'TEXT'          Update 64-char RadioText\n");
    printf("                            ctl track 'Title' 'Artist'  Update RT+ metadata\n");
    printf("                            ctl ta <on|off>        Toggle Traffic Announcement\n");
    printf("                            ctl stop               Safely terminate broadcast\n");
    printf("  status                  Probe and display detected hardware capabilities\n\n");

    printf("Standard Options:\n");
    printf("  -f, --freq <MHz>        Carrier frequency in MHz (76.0 - 108.0, default: 107.9)\n");
    printf("  -a, --audio <file>      Audio file (WAV) or '-' for stdin streaming (PipeWire/ffmpeg)\n");
    printf("  -s, --station, --ps     Station Name (8 chars, e.g. 'ENCHNTED')\n");
    printf("      --dynamic-ps <text> Long station text for dynamic paging / scrolling\n");
    printf("      --rt, --text <text> RadioText string (up to 64 characters)\n");
    printf("      --title, --song     Track title for RT+ tagging\n");
    printf("      --artist, --band    Artist name for RT+ tagging\n");
    printf("      --mono / --stereo   Toggle stereo modulation (default: stereo)\n");
    printf("      --loop              Loop audio file continuously\n");
    printf("      --gain <float>      Audio volume gain factor (default: 1.0)\n");
    printf("      --preemph <50|75|0> Pre-emphasis curve (default: 50 us Europe/Asia, 75 us US)\n");
    printf("      --config <path>     Path to custom config file (default: ~/.config/pifm/pifm.conf)\n\n");

    printf("SDR & Baseband Output (For Raspberry Pi 5, PC, Mac, HackRF, FL2k):\n");
    printf("      --iq-out <file|->   Stream complex I/Q samples to stdout ('-') or file for SDR\n");
    printf("      --iq-rate <sps>     SDR sample rate in Hz (default: 2000000 = 2.0 MSPS)\n");
    printf("      --iq-format <fmt>   Format: 's8' (HackRF), 'u8' (FL2k), 's16' (LimeSDR), 'f32'\n");
    printf("  -o, --wav-out <file>    Output composite MPX to 228 kHz WAV file\n\n");

    printf("💡 Super-simple Examples:\n");
    printf("  # Just play on default 107.9 MHz:\n");
    printf("  sudo %s sound.wav\n\n", prog_name);
    printf("  # Play on 98.5 MHz with station name:\n");
    printf("  sudo %s sound.wav 98.5 --station 'HITS'\n\n", prog_name);
    printf("  # Stream to HackRF on Raspberry Pi 5 / PC:\n");
    printf("  %s sound.wav --iq-out - | hackrf_transfer -t - -f 107900000 -s 2000000 -a 1 -x 20\n\n", prog_name);
    printf("  # Update song title in real time without killing process:\n");
    printf("  %s ctl track 'Blinding Lights' 'The Weeknd'\n\n", prog_name);
}

static int parse_pty_argument(const char *arg) {
    if (!arg) return 10;
    char *endptr = NULL;
    long val = strtol(arg, &endptr, 10);
    if (endptr != arg && *endptr == '\0') {
        if (val < 0) val = 0;
        if (val > 31) val = 31;
        return (int)val;
    }

    /* Standard RDS genre lookup */
    if (strcasecmp(arg, "news") == 0) return 1;
    if (strcasecmp(arg, "affairs") == 0) return 2;
    if (strcasecmp(arg, "info") == 0) return 3;
    if (strcasecmp(arg, "sport") == 0 || strcasecmp(arg, "sports") == 0) return 4;
    if (strcasecmp(arg, "education") == 0) return 5;
    if (strcasecmp(arg, "drama") == 0) return 6;
    if (strcasecmp(arg, "culture") == 0) return 7;
    if (strcasecmp(arg, "science") == 0) return 8;
    if (strcasecmp(arg, "varied") == 0 || strcasecmp(arg, "talk") == 0) return 9;
    if (strcasecmp(arg, "pop") == 0) return 10;
    if (strcasecmp(arg, "rock") == 0) return 11;
    if (strcasecmp(arg, "easy") == 0) return 12;
    if (strcasecmp(arg, "light") == 0) return 13;
    if (strcasecmp(arg, "classics") == 0 || strcasecmp(arg, "classical") == 0) return 14;
    if (strcasecmp(arg, "other") == 0) return 15;
    if (strcasecmp(arg, "weather") == 0) return 16;
    if (strcasecmp(arg, "finance") == 0) return 17;
    if (strcasecmp(arg, "children") == 0) return 18;
    if (strcasecmp(arg, "social") == 0) return 19;
    if (strcasecmp(arg, "religion") == 0) return 20;
    if (strcasecmp(arg, "phone_in") == 0) return 21;
    if (strcasecmp(arg, "travel") == 0) return 22;
    if (strcasecmp(arg, "leisure") == 0) return 23;
    if (strcasecmp(arg, "jazz") == 0) return 24;
    if (strcasecmp(arg, "country") == 0) return 25;
    if (strcasecmp(arg, "nation") == 0) return 26;
    if (strcasecmp(arg, "oldies") == 0) return 27;
    if (strcasecmp(arg, "folk") == 0) return 28;
    if (strcasecmp(arg, "documentary") == 0) return 29;

    return 10;
}

static int handle_ctl_subcommand(int argc, char **argv, const char *sock_path, const char *fifo_path) {
    if (argc < 3) {
        fprintf(stderr, "Error: missing ctl subcommand.\nSyntax: %s ctl [ps|dynamic|rt|track|ta|stop] <args>\n", argv[0]);
        return 1;
    }

    char cmd_buffer[256] = {0};
    const char *sub = argv[2];

    if (strcasecmp(sub, "ps") == 0 && argc >= 4) {
        snprintf(cmd_buffer, sizeof(cmd_buffer), "PS %s", argv[3]);
    } else if (strcasecmp(sub, "dynamic") == 0 && argc >= 4) {
        snprintf(cmd_buffer, sizeof(cmd_buffer), "DYNAMIC_PS page 2000 %s", argv[3]);
    } else if (strcasecmp(sub, "rt") == 0 && argc >= 4) {
        snprintf(cmd_buffer, sizeof(cmd_buffer), "RT %s", argv[3]);
    } else if (strcasecmp(sub, "track") == 0 && argc >= 4) {
        const char *title = argv[3];
        const char *artist = (argc >= 5) ? argv[4] : "";
        snprintf(cmd_buffer, sizeof(cmd_buffer), "TRACK %s | %s", title, artist);
    } else if (strcasecmp(sub, "ta") == 0 && argc >= 4) {
        snprintf(cmd_buffer, sizeof(cmd_buffer), "TA %s", argv[3]);
    } else if (strcasecmp(sub, "stop") == 0 || strcasecmp(sub, "quit") == 0) {
        snprintf(cmd_buffer, sizeof(cmd_buffer), "QUIT");
    } else {
        fprintf(stderr, "Unknown ctl command: %s\n", sub);
        return 1;
    }

    /* First attempt to send via UNIX domain datagram socket */
    bool sent = false;
    if (sock_path && strlen(sock_path) > 0) {
        int sock = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (sock >= 0) {
            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);
            if (sendto(sock, cmd_buffer, strlen(cmd_buffer), 0, (struct sockaddr *)&addr, sizeof(addr)) > 0) {
                sent = true;
            }
            close(sock);
        }
    }

    /* Fallback to FIFO if socket was not available */
    if (!sent && fifo_path && strlen(fifo_path) > 0) {
        int fd = open(fifo_path, O_WRONLY | O_NONBLOCK);
        if (fd >= 0) {
            ssize_t written = write(fd, cmd_buffer, strlen(cmd_buffer));
            (void)write(fd, "\n", 1);
            close(fd);
            if (written > 0) sent = true;
        }
    }

    if (!sent) {
        fprintf(stderr, "[Error] Could not communicate with transmitter via socket (%s) or FIFO (%s).\n"
                        "Is PiFmRds-Enchanted currently transmitting?\n",
                sock_path ? sock_path : "none", fifo_path ? fifo_path : "none");
        return 1;
    }

    printf("[OK] Sent: %s\n", cmd_buffer);
    return 0;
}

int main(int argc, char **argv) {
    setlocale(LC_ALL, "");

    /* Load user configuration file (~/.config/pifm/pifm.conf or /etc/pifm.conf) */
    pifm_app_config_t app_cfg;
    pifm_config_load(&app_cfg, NULL);

    /* Quick check for 'status', 'ver', 'version' or 'ctl' subcommands */
    if (argc >= 2) {
        if (strcasecmp(argv[1], "ver") == 0 ||
            strcasecmp(argv[1], "version") == 0 ||
            strcasecmp(argv[1], "-v") == 0 ||
            strcasecmp(argv[1], "--version") == 0) {
            print_version();
            return 0;
        }
        if (strcasecmp(argv[1], "status") == 0 || strcasecmp(argv[1], "--detect") == 0) {
            rpi_hw_info_t hw;
            rpi_hw_detect(&hw);
            printf("============================================================\n");
            printf("PiFmRds-Enchanted Hardware Status\n");
            printf("Detected Hardware: %s\n", hw.model_name);
            printf("Direct RF Support: %s\n", hw.direct_rf_supported ? "YES (GPIO 4 / Pin 7)" : "NO (Use SDR I/Q mode)");
            printf("Note: %s\n", hw.compatibility_note);
            printf("============================================================\n");
            return 0;
        }
        if (strcasecmp(argv[1], "ctl") == 0) {
            if (argc >= 3 && (strcasecmp(argv[2], "ver") == 0 || strcasecmp(argv[2], "version") == 0)) {
                print_version();
                return 0;
            }
            return handle_ctl_subcommand(argc, argv, app_cfg.default_sock, app_cfg.default_pipe);
        }
    }

    /* Default settings from configuration */
    double carrier_freq_mhz = app_cfg.default_freq;
    float ppm = 0.0f;
    const char *audio_file = NULL;
    const char *fifo_path = app_cfg.default_pipe;
    const char *sock_path = app_cfg.default_sock;
    const char *wav_out_path = NULL;
    const char *iq_out_path = NULL;
    uint32_t iq_rate = 2000000;
    iq_format_t iq_fmt = IQ_FORMAT_S8;
    bool is_stereo = app_cfg.default_stereo;
    bool loop_audio = false;
    float audio_gain = app_cfg.default_gain;
    mpx_pre_emphasis_t pre_emph = (app_cfg.default_preemph == 75) ? PRE_EMPHASIS_75US :
                                  (app_cfg.default_preemph == 0) ? PRE_EMPHASIS_NONE : PRE_EMPHASIS_50US;

    uint16_t pi_code = app_cfg.default_pi;
    const char *ps_text = app_cfg.default_ps;
    const char *dynamic_ps_text = NULL;
    rds_ps_mode_t ps_mode = RDS_PS_MODE_PAGE;
    uint32_t ps_interval_ms = 2000;
    const char *rt_text = app_cfg.default_rt;
    const char *title_text = NULL;
    const char *artist_text = NULL;
    int pty_code = 10;
    rds_pty_standard_t pty_std = RDS_PTY_STANDARD_RDS;
    bool ta_flag = false;
    bool tp_flag = true;
    bool enable_ct = true;

    /* Smart Positional Arguments Auto-detection:
       Support: 'pifm sound.wav', 'pifm 107.9 sound.wav', 'pifm sound.wav 107.9', 'pifm play sound.wav' */
    int start_arg = 1;
    if (argc >= 2 && strcasecmp(argv[1], "play") == 0) {
        start_arg = 2;
    }

    for (int i = start_arg; i < argc; i++) {
        if (argv[i][0] != '-') {
            char *endptr = NULL;
            double maybe_freq = strtod(argv[i], &endptr);
            if (endptr != argv[i] && *endptr == '\0' && maybe_freq >= 76.0 && maybe_freq <= 108.0) {
                carrier_freq_mhz = maybe_freq;
            } else if (!audio_file) {
                audio_file = argv[i];
            }
        }
    }

    static struct option long_options[] = {
        {"freq",        required_argument, 0, 'f'},
        {"frequency",   required_argument, 0, 'f'},
        {"ppm",         required_argument, 0, 1001},
        {"audio",       required_argument, 0, 'a'},
        {"file",        required_argument, 0, 'a'},
        {"mono",        no_argument,       0, 1003},
        {"stereo",      no_argument,       0, 1004},
        {"preemph",     required_argument, 0, 1005},
        {"gain",        required_argument, 0, 1006},
        {"loop",        no_argument,       0, 1007},
        {"pi",          required_argument, 0, 1008},
        {"ps",          required_argument, 0, 's'},
        {"station",     required_argument, 0, 's'},
        {"dynamic-ps",  required_argument, 0, 1010},
        {"ps-mode",     required_argument, 0, 1011},
        {"ps-rate",     required_argument, 0, 1012},
        {"rt",          required_argument, 0, 1013},
        {"text",        required_argument, 0, 1013},
        {"title",       required_argument, 0, 1014},
        {"song",        required_argument, 0, 1014},
        {"artist",      required_argument, 0, 1015},
        {"band",        required_argument, 0, 1015},
        {"pty",         required_argument, 0, 1016},
        {"rbds",        no_argument,       0, 1017},
        {"ta",          no_argument,       0, 1018},
        {"tp",          no_argument,       0, 1019},
        {"no-ct",       no_argument,       0, 1020},
        {"ctl",         required_argument, 0, 1021},
        {"sock",        required_argument, 0, 1022},
        {"wav-out",     required_argument, 0, 'o'},
        {"iq-out",      required_argument, 0, 1023},
        {"iq-rate",     required_argument, 0, 1024},
        {"iq-format",   required_argument, 0, 1025},
        {"config",      required_argument, 0, 1026},
        {"help",        no_argument,       0, 'h'},
        {"version",     no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };

    int opt;
    optind = 1;
    while ((opt = getopt_long(argc, argv, "f:a:s:o:hv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'f': carrier_freq_mhz = atof(optarg); break;
            case 1001: ppm = (float)atof(optarg); break;
            case 'a': audio_file = optarg; break;
            case 1003: is_stereo = false; break;
            case 1004: is_stereo = true; break;
            case 1005: {
                int pe = atoi(optarg);
                if (pe == 75) pre_emph = PRE_EMPHASIS_75US;
                else if (pe == 0) pre_emph = PRE_EMPHASIS_NONE;
                else pre_emph = PRE_EMPHASIS_50US;
                break;
            }
            case 1006: audio_gain = (float)atof(optarg); break;
            case 1007: loop_audio = true; break;
            case 1008: pi_code = (uint16_t)strtol(optarg, NULL, 16); break;
            case 's': ps_text = optarg; break;
            case 1010: dynamic_ps_text = optarg; break;
            case 1011:
                if (strcasecmp(optarg, "scroll") == 0) ps_mode = RDS_PS_MODE_SCROLL;
                else ps_mode = RDS_PS_MODE_PAGE;
                break;
            case 1012: ps_interval_ms = (uint32_t)atoi(optarg); break;
            case 1013: rt_text = optarg; break;
            case 1014: title_text = optarg; break;
            case 1015: artist_text = optarg; break;
            case 1016: pty_code = parse_pty_argument(optarg); break;
            case 1017: pty_std = RDS_PTY_STANDARD_RBDS; break;
            case 1018: ta_flag = true; break;
            case 1019: tp_flag = true; break;
            case 1020: enable_ct = false; break;
            case 1021: fifo_path = optarg; break;
            case 1022: sock_path = optarg; break;
            case 'o': wav_out_path = optarg; break;
            case 1023: iq_out_path = optarg; break;
            case 1024: iq_rate = (uint32_t)atoi(optarg); break;
            case 1025:
                if (strcasecmp(optarg, "u8") == 0) iq_fmt = IQ_FORMAT_U8;
                else if (strcasecmp(optarg, "s16") == 0) iq_fmt = IQ_FORMAT_S16_LE;
                else if (strcasecmp(optarg, "f32") == 0) iq_fmt = IQ_FORMAT_FLOAT32;
                else iq_fmt = IQ_FORMAT_S8;
                break;
            case 1026: pifm_config_load(&app_cfg, optarg); break;
            case 'v': print_version(); return 0;
            case 'h': print_usage(argv[0]); return 0;
            default: break;
        }
    }

    /* Probe hardware */
    rpi_hw_info_t hw_info;
    rpi_hw_detect(&hw_info);

    bool is_software_output = (wav_out_path != NULL) || (iq_out_path != NULL);

    if (!is_software_output && !hw_info.direct_rf_supported) {
        if (hw_info.soc == RPI_SOC_BCM2712) {
            fprintf(stderr, "\n[FATAL] Raspberry Pi 5 detected!\n"
                            "Direct GPIO RF modulation is not supported on Pi 5 because GPIOs run over PCIe/RP1.\n"
                            "To transmit on Pi 5 or PC:\n"
                            "  %s %s --iq-out - | hackrf_transfer -t - -f 107900000 -s 2000000\n\n",
                            argv[0], audio_file ? audio_file : "sound.wav");
        } else {
            fprintf(stderr, "\n[FATAL] Direct GPIO RF is only available on Raspberry Pi (models 1, 2, 3, 4, Zero).\n"
                            "To run this on PC/Mac/Pi 5, use '--iq-out -' for SDR or '-o <file.wav>' for MPX WAV.\n\n");
        }
        return 1;
    }

    /* Signal handlers for clean shutdown */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    /* Initialize RDS */
    rds_init();
    set_rds_pi(pi_code);
    set_rds_pty((uint8_t)pty_code, pty_std);
    set_rds_ta(ta_flag);
    set_rds_tp(tp_flag);
    set_rds_ct(enable_ct);

    if (dynamic_ps_text) {
        set_rds_dynamic_ps(dynamic_ps_text, ps_mode, ps_interval_ms);
    } else {
        set_rds_ps(ps_text);
    }

    if (title_text || artist_text) {
        set_rds_rt_plus(title_text, artist_text);
    } else {
        set_rds_rt(rt_text);
    }

    /* Initialize MPX Generator with Broadcast DSP */
    mpx_config_t mpx_cfg;
    memset(&mpx_cfg, 0, sizeof(mpx_cfg));
    mpx_cfg.audio_source = audio_file;
    mpx_cfg.is_stereo = is_stereo;
    mpx_cfg.pre_emph = pre_emph;
    mpx_cfg.audio_gain = audio_gain;
    mpx_cfg.pilot_level = 0.09f;
    mpx_cfg.rds_level = 0.05f;
    mpx_cfg.rds_enabled = true;
    mpx_cfg.loop_audio = loop_audio;

    if (fm_mpx_init(&mpx_cfg, BLOCK_SIZE) < 0) {
        fprintf(stderr, "[FATAL] Failed to initialize FM multiplex generator.\n");
        return 1;
    }

    /* Setup output mode: SDR I/Q, baseband WAV, or Direct RF */
    if (iq_out_path) {
        g_is_iq_mode = true;
        iq_config_t iq_conf;
        memset(&iq_conf, 0, sizeof(iq_conf));
        iq_conf.out_sample_rate = iq_rate;
        iq_conf.deviation_hz = 75000.0;
        iq_conf.format = iq_fmt;

        if (strcmp(iq_out_path, "-") == 0 || strcmp(iq_out_path, "stdout") == 0) {
            iq_conf.out_fp = stdout;
            iq_conf.is_stdout = true;
        } else {
            iq_conf.out_fp = fopen(iq_out_path, "wb");
            iq_conf.is_stdout = false;
        }

        if (!iq_conf.out_fp) {
            fprintf(stderr, "[FATAL] Could not open SDR I/Q destination '%s'.\n", iq_out_path);
            fm_mpx_close();
            return 1;
        }

        iq_modulator_init(&g_iq_mod, &iq_conf);
        if (!iq_conf.is_stdout) {
            printf("[SDR] Streaming I/Q to '%s' (%u SPS)\n", iq_out_path, iq_rate);
        }
    } else if (wav_out_path) {
        g_wav_out = wav_open_write(wav_out_path, MPX_SAMPLE_RATE, 1, 16);
        if (!g_wav_out) {
            fprintf(stderr, "[FATAL] Could not open WAV output file '%s'.\n", wav_out_path);
            fm_mpx_close();
            return 1;
        }
        printf("[Output] Writing MPX stream to '%s' at %d Hz mono...\n", wav_out_path, MPX_SAMPLE_RATE);
    } else {
        uint32_t carrier_hz = (uint32_t)(carrier_freq_mhz * 1.0e6);
        if (rpi_hw_init(&hw_info, carrier_hz, ppm) < 0) {
            fprintf(stderr, "[FATAL] Hardware initialization failed. Are you root ('sudo')?\n");
            fm_mpx_close();
            return 1;
        }
    }

    if (fifo_path || sock_path) {
        control_init(fifo_path, sock_path);
    }

    /* Print Clean Status Banner if not streaming raw binary to stdout */
    if (!g_is_iq_mode || !g_iq_mod.config.is_stdout) {
        printf("\n============================================================\n");
        printf("📻 PiFmRds-Enchanted (Wydanie 2026)\n");
        printf("   Częstotliwość: %.2f MHz | Tryb: %s\n", carrier_freq_mhz, is_stereo ? "Stereo MPX" : "Mono");
        printf("   Stacja (PS):   [%s]\n", ps_text);
        printf("   RadioText:     \"%s\"\n", rt_text);
        if (audio_file) printf("   Plik audio:    %s\n", audio_file);
        printf("============================================================\n");
        printf("Nadawanie aktywne. Naciśnij Ctrl+C, aby zakończyć bezpiecznie.\n\n");
    }

    float mpx_buffer[BLOCK_SIZE];
    uint32_t tick = 0;

    while (g_running) {
        ctl_command_t cmd;
        if (control_poll(&cmd)) {
            switch (cmd.type) {
                case CTL_CMD_PS_SET:
                    set_rds_ps(cmd.text_arg1);
                    break;
                case CTL_CMD_DYNAMIC_PS_SET:
                    set_rds_dynamic_ps(cmd.text_arg1, (rds_ps_mode_t)cmd.int_arg, (uint32_t)cmd.float_arg);
                    break;
                case CTL_CMD_RT_SET:
                    set_rds_rt(cmd.text_arg1);
                    break;
                case CTL_CMD_RT_PLUS_SET:
                    set_rds_rt_plus(cmd.text_arg1, cmd.text_arg2);
                    break;
                case CTL_CMD_TA_SET:
                    set_rds_ta(cmd.int_arg != 0);
                    break;
                case CTL_CMD_TP_SET:
                    set_rds_tp(cmd.int_arg != 0);
                    break;
                case CTL_CMD_PTY_SET:
                    set_rds_pty((uint8_t)cmd.int_arg, pty_std);
                    break;
                case CTL_CMD_SHUTDOWN:
                    g_running = 0;
                    break;
                default:
                    break;
            }
        }

        int samples = fm_mpx_get_samples(mpx_buffer, BLOCK_SIZE);
        if (samples <= 0) {
            if (fm_mpx_is_eof() && !loop_audio) {
                break;
            }
            usleep(10000);
            continue;
        }

        if (g_is_iq_mode) {
            iq_modulator_process(&g_iq_mod, mpx_buffer, samples);
            if (fm_mpx_is_eof() && !loop_audio) break;
        } else if (g_wav_out) {
            wav_write_float_frames(g_wav_out, mpx_buffer, samples);
            if (fm_mpx_is_eof()) break;
        } else {
            int written = 0;
            while (written < samples && g_running) {
                int w = rpi_hw_write_samples(&mpx_buffer[written], samples - written, 75.0f);
                if (w > 0) {
                    written += w;
                } else {
                    usleep(1000);
                }
            }
        }

        /* Live console ticker (every 50 iterations ~ 500 ms) */
        if ((++tick % 50 == 0) && (!g_is_iq_mode || !g_iq_mod.config.is_stdout) && isatty(fileno(stdout))) {
            const rds_config_t *c = rds_get_config();
            float peak = 0.0f;
            for (int i = 0; i < samples; i++) {
                float v = fabsf(mpx_buffer[i]);
                if (v > peak) peak = v;
            }
            int bars = (int)(peak * 10.0f);
            if (bars > 10) bars = 10;
            char vu[11];
            for (int b = 0; b < 10; b++) vu[b] = (b < bars) ? '#' : '-';
            vu[10] = '\0';

            printf("\r[📻 %.1f MHz] [PS: %-8.8s] [VU: %s] [TA: %s]  ",
                   carrier_freq_mhz, c->ps, vu, c->ta ? "ON " : "OFF");
            fflush(stdout);
        }
    }

    if (!g_is_iq_mode || !g_iq_mod.config.is_stdout) {
        printf("\n\n[PiFmRds-Enchanted] Zamykanie nadajnika i czyszczenie zasobów...\n");
    }

    control_cleanup();
    if (g_is_iq_mode) {
        iq_modulator_close(&g_iq_mod);
    } else if (g_wav_out) {
        wav_close(g_wav_out);
        g_wav_out = NULL;
    } else {
        rpi_hw_shutdown();
    }
    fm_mpx_close();
    rds_cleanup();

    if (!g_is_iq_mode || !g_iq_mod.config.is_stdout) {
        printf("[PiFmRds-Enchanted] Zakończono pomyślnie.\n");
    }
    return 0;
}
