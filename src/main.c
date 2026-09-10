/*
 * PiFmRds-Enchanted - FM/RDS Transmitter (2026 Edition)
 *
 * Main Application CLI Entry Point with Direct RF, MPX WAV, and SDR I/Q Streaming
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <locale.h>
#include "rds.h"
#include "fm_mpx.h"
#include "hw_rpi.h"
#include "control.h"
#include "wav_io.h"
#include "iq_modulator.h"

#define PIFMRDS_VERSION "2.0.0-enchanted-2026"
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
    printf("PiFmRds-Enchanted version %s (Built for 2026 standards)\n", PIFMRDS_VERSION);
    printf("Original authors: Christophe Jacquet, Richard Hirst, Oliver Mattos\n");
    printf("Enchanted Edition: Broadcast DSP (15kHz LPF + AGC), SDR I/Q streaming, RT+, Dynamic PS\n");
}

static void print_usage(const char *prog_name) {
    print_version();
    printf("\nUsage:\n");
    printf("  %s [options]\n\n", prog_name);
    printf("Frequency & Hardware:\n");
    printf("  -f, --freq <MHz>        Carrier frequency in MHz (76.0 - 108.0, default: 107.9)\n");
    printf("      --ppm <val>         Oscillator error correction in PPM (default: 0.0)\n");
    printf("      --detect            Probe and display detected hardware capabilities and exit\n\n");
    printf("Audio & Modulation:\n");
    printf("  -a, --audio <file>      Audio file (WAV) or '-' for stdin streaming (PipeWire/ffmpeg)\n");
    printf("      --mono              Transmit in mono (disables 19 kHz pilot and 38 kHz L-R)\n");
    printf("      --stereo            Transmit in stereo (default)\n");
    printf("      --preemph <us>      Pre-emphasis: 50 (Europe/Asia), 75 (Americas), or 0 (none)\n");
    printf("      --gain <float>      Audio gain factor (default: 1.0)\n");
    printf("      --loop              Loop audio file continuously\n\n");
    printf("RDS / RBDS Features:\n");
    printf("      --pi <hex>          Program Identification code (e.g. 0x3201 or 0x1234)\n");
    printf("      --ps <text>         Station Name (8 chars, e.g. 'ENCHNTED')\n");
    printf("      --dynamic-ps <text> Long station text for dynamic paging / scrolling\n");
    printf("      --ps-mode <mode>    Dynamic PS mode: 'page' (by words) or 'scroll'\n");
    printf("      --ps-rate <ms>      Dynamic PS update interval in milliseconds (default: 2000)\n");
    printf("      --rt <text>         RadioText (up to 64 characters)\n");
    printf("      --title <title>     Track title for RT+ tagging\n");
    printf("      --artist <artist>   Artist name for RT+ tagging\n");
    printf("      --pty <0-31>        Program Type number (default: 10 = Pop)\n");
    printf("      --rbds              Use North American RBDS PTY names\n");
    printf("      --ta                Enable Traffic Announcement flag\n");
    printf("      --tp                Enable Traffic Programme flag\n");
    printf("      --no-ct             Disable automatic Clock-Time broadcasting (Group 4A)\n\n");
    printf("SDR & Baseband Output (Universal for Raspberry Pi 5, PC, Mac, HackRF, FL2k):\n");
    printf("      --iq-out <file|->   Stream complex I/Q samples to file or stdout ('-') for SDR\n");
    printf("      --iq-rate <sps>     SDR sample rate in Hz (default: 2000000 = 2.0 MSPS)\n");
    printf("      --iq-format <fmt>   I/Q format: 's8' (HackRF), 'u8' (FL2k), 's16' (LimeSDR), 'f32'\n");
    printf("  -o, --wav-out <file>    Output composite MPX to 228 kHz WAV file\n\n");
    printf("Control & IPC:\n");
    printf("      --ctl <pipe_path>   Named pipe (FIFO) for real-time control\n");
    printf("      --sock <path>       UNIX domain socket for real-time control\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -v, --version           Show version\n\n");
    printf("Examples:\n");
    printf("  # 1. Direct RF on Raspberry Pi 1-4:\n");
    printf("  sudo %s -f 107.9 -a sound.wav --ps 'ENCHNTED' --rt 'PiFmRds-Enchanted 2026'\n\n", prog_name);
    printf("  # 2. SDR Transmission on Raspberry Pi 5 or PC via HackRF:\n");
    printf("  %s -a sound.wav --ps 'ENCHNTED' --iq-out - | hackrf_transfer -t - -f 107900000 -s 2000000 -a 1 -x 20\n\n", prog_name);
    printf("  # 3. Stream from PipeWire / ffmpeg:\n");
    printf("  ffmpeg -i https://stream.radio.example/live.mp3 -f s16le -ar 44100 -ac 2 - | sudo %s -f 107.9 -a -\n\n", prog_name);
}

int main(int argc, char **argv) {
    setlocale(LC_ALL, "");

    /* Default settings */
    double carrier_freq_mhz = 107.9;
    float ppm = 0.0f;
    const char *audio_file = NULL;
    const char *fifo_path = NULL;
    const char *sock_path = NULL;
    const char *wav_out_path = NULL;
    const char *iq_out_path = NULL;
    uint32_t iq_rate = 2000000;
    iq_format_t iq_fmt = IQ_FORMAT_S8;
    bool detect_only = false;
    bool is_stereo = true;
    bool loop_audio = false;
    float audio_gain = 1.0f;
    mpx_pre_emphasis_t pre_emph = PRE_EMPHASIS_50US;

    uint16_t pi_code = 0x1234;
    const char *ps_text = "ENCHNTED";
    const char *dynamic_ps_text = NULL;
    rds_ps_mode_t ps_mode = RDS_PS_MODE_PAGE;
    uint32_t ps_interval_ms = 2000;
    const char *rt_text = "PiFmRds-Enchanted: Next-Gen FM/RDS Transmitter";
    const char *title_text = NULL;
    const char *artist_text = NULL;
    int pty_code = 10;
    rds_pty_standard_t pty_std = RDS_PTY_STANDARD_RDS;
    bool ta_flag = false;
    bool tp_flag = true;
    bool enable_ct = true;

    static struct option long_options[] = {
        {"freq",        required_argument, 0, 'f'},
        {"ppm",         required_argument, 0, 1001},
        {"detect",      no_argument,       0, 1002},
        {"audio",       required_argument, 0, 'a'},
        {"mono",        no_argument,       0, 1003},
        {"stereo",      no_argument,       0, 1004},
        {"preemph",     required_argument, 0, 1005},
        {"gain",        required_argument, 0, 1006},
        {"loop",        no_argument,       0, 1007},
        {"pi",          required_argument, 0, 1008},
        {"ps",          required_argument, 0, 1009},
        {"dynamic-ps",  required_argument, 0, 1010},
        {"ps-mode",     required_argument, 0, 1011},
        {"ps-rate",     required_argument, 0, 1012},
        {"rt",          required_argument, 0, 1013},
        {"title",       required_argument, 0, 1014},
        {"artist",      required_argument, 0, 1015},
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
        {"help",        no_argument,       0, 'h'},
        {"version",     no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "f:a:o:hv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'f': carrier_freq_mhz = atof(optarg); break;
            case 1001: ppm = (float)atof(optarg); break;
            case 1002: detect_only = true; break;
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
            case 1009: ps_text = optarg; break;
            case 1010: dynamic_ps_text = optarg; break;
            case 1011:
                if (strcasecmp(optarg, "scroll") == 0) ps_mode = RDS_PS_MODE_SCROLL;
                else ps_mode = RDS_PS_MODE_PAGE;
                break;
            case 1012: ps_interval_ms = (uint32_t)atoi(optarg); break;
            case 1013: rt_text = optarg; break;
            case 1014: title_text = optarg; break;
            case 1015: artist_text = optarg; break;
            case 1016: pty_code = atoi(optarg); break;
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
            case 'v': print_version(); return 0;
            case 'h': print_usage(argv[0]); return 0;
            default: print_usage(argv[0]); return 1;
        }
    }

    /* Probe hardware */
    rpi_hw_info_t hw_info;
    rpi_hw_detect(&hw_info);

    printf("============================================================\n");
    printf("PiFmRds-Enchanted (2026 Edition)\n");
    printf("Detected Hardware: %s\n", hw_info.model_name);
    printf("Direct RF Support: %s\n", hw_info.direct_rf_supported ? "YES" : "NO");
    printf("Note: %s\n", hw_info.compatibility_note);
    printf("============================================================\n");

    if (detect_only) {
        return 0;
    }

    if (carrier_freq_mhz < 76.0 || carrier_freq_mhz > 108.0) {
        fprintf(stderr, "Error: Carrier frequency must be between 76.0 and 108.0 MHz (got %.2f).\n", carrier_freq_mhz);
        return 1;
    }

    bool is_software_output = (wav_out_path != NULL) || (iq_out_path != NULL);

    if (!is_software_output && !hw_info.direct_rf_supported) {
        if (hw_info.soc == RPI_SOC_BCM2712) {
            fprintf(stderr, "\n[FATAL] Raspberry Pi 5 detected!\n"
                            "Direct GPIO RF modulation is not supported on the Pi 5 because all 40 GPIO pins\n"
                            "are handled by the RP1 southbridge chip over PCIe, lacking direct SoC GPCLK0 access.\n"
                            "\nTo transmit using this engine on Pi 5 or PC:\n"
                            "  1) Use direct SDR streaming via '--iq-out -' (HackRF, LimeSDR, FL2k):\n"
                            "     %s -a %s --iq-out - | hackrf_transfer -t - -f 107900000 -s 2000000\n"
                            "  2) Or export composite MPX using '-o <file.wav>' for external modulation:\n"
                            "     %s -a %s -o mpx_output.wav\n\n",
                            argv[0], audio_file ? audio_file : "sound.wav",
                            argv[0], audio_file ? audio_file : "sound.wav");
        } else {
            fprintf(stderr, "\n[FATAL] Direct GPIO RF is only available on Raspberry Pi hardware (models 1, 2, 3, 4, Zero).\n"
                            "To run this on PC/Mac/Pi 5, use '--iq-out -' for SDR or '-o <file.wav>' to generate baseband MPX WAV.\n\n");
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
        printf("[RDS] Dynamic PS enabled: \"%s\" (%s mode, %u ms interval)\n",
               dynamic_ps_text, (ps_mode == RDS_PS_MODE_SCROLL) ? "scroll" : "paged", ps_interval_ms);
    } else {
        set_rds_ps(ps_text);
        printf("[RDS] Static PS: \"%s\"\n", ps_text);
    }

    if (title_text || artist_text) {
        set_rds_rt_plus(title_text, artist_text);
        printf("[RDS] RT+ Enabled: Artist=\"%s\", Title=\"%s\"\n",
               artist_text ? artist_text : "", title_text ? title_text : "");
    } else {
        set_rds_rt(rt_text);
        printf("[RDS] RadioText: \"%s\"\n", rt_text);
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
        fprintf(stderr, "[SDR] Modulated I/Q streaming active at %u SPS (%s format)\n",
                iq_rate, (iq_fmt == IQ_FORMAT_S8 ? "8-bit signed" :
                         iq_fmt == IQ_FORMAT_U8 ? "8-bit unsigned" :
                         iq_fmt == IQ_FORMAT_S16_LE ? "16-bit signed LE" : "32-bit float"));
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

    if (!g_is_iq_mode || !g_iq_mod.config.is_stdout) {
        printf("[PiFmRds-Enchanted] Transmission active. Press Ctrl+C to terminate cleanly.\n");
    }

    float mpx_buffer[BLOCK_SIZE];
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
    }

    if (!g_is_iq_mode || !g_iq_mod.config.is_stdout) {
        printf("\n[PiFmRds-Enchanted] Cleaning up and shutting down...\n");
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
        printf("[PiFmRds-Enchanted] Clean shutdown complete.\n");
    }
    return 0;
}
