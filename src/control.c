/*
 * PiFmRds-ng - Next-Generation FM/RDS transmitter
 * Modernized for 2026 standards
 *
 * Real-time Control Server (UNIX Domain Socket & FIFO Named Pipe)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "control.h"

#define CTL_LINE_MAX 256

static int g_fifo_fd = -1;
static int g_sock_fd = -1;
static char g_fifo_path[256] = {0};
static char g_sock_path[256] = {0};

static char g_cached_title[128] = {0};
static char g_cached_artist[128] = {0};

int control_init(const char *fifo_path, const char *socket_path) {
    if (fifo_path && strlen(fifo_path) > 0) {
        strncpy(g_fifo_path, fifo_path, sizeof(g_fifo_path) - 1);
        mkfifo(g_fifo_path, 0666);
        /* Open non-blocking read-write so open doesn't block waiting for writer */
        g_fifo_fd = open(g_fifo_path, O_RDWR | O_NONBLOCK);
        if (g_fifo_fd >= 0) {
            printf("[Control] Listening on FIFO pipe: %s\n", g_fifo_path);
        } else {
            perror("[Control] Failed to open FIFO pipe");
        }
    }

    if (socket_path && strlen(socket_path) > 0) {
        strncpy(g_sock_path, socket_path, sizeof(g_sock_path) - 1);
        unlink(g_sock_path);

        g_sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (g_sock_fd >= 0) {
            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, g_sock_path, sizeof(addr.sun_path) - 1);

            int flags = fcntl(g_sock_fd, F_GETFL, 0);
            fcntl(g_sock_fd, F_SETFL, flags | O_NONBLOCK);

            if (bind(g_sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
                chmod(g_sock_path, 0666);
                printf("[Control] Listening on UNIX domain socket: %s\n", g_sock_path);
            } else {
                perror("[Control] Failed to bind UNIX domain socket");
                close(g_sock_fd);
                g_sock_fd = -1;
            }
        }
    }

    return (g_fifo_fd >= 0 || g_sock_fd >= 0) ? 0 : -1;
}

#define CTL_QUEUE_SIZE 16
static char g_line_queue[CTL_QUEUE_SIZE][CTL_LINE_MAX];
static int g_q_head = 0;
static int g_q_tail = 0;

static void queue_push(const char *line) {
    if (!line || *line == '\0') return;
    int next = (g_q_tail + 1) % CTL_QUEUE_SIZE;
    if (next != g_q_head) {
        strncpy(g_line_queue[g_q_tail], line, CTL_LINE_MAX - 1);
        g_line_queue[g_q_tail][CTL_LINE_MAX - 1] = '\0';
        g_q_tail = next;
    }
}

static bool queue_pop(char *out_line) {
    if (g_q_head == g_q_tail) return false;
    strncpy(out_line, g_line_queue[g_q_head], CTL_LINE_MAX - 1);
    out_line[CTL_LINE_MAX - 1] = '\0';
    g_q_head = (g_q_head + 1) % CTL_QUEUE_SIZE;
    return true;
}

static void ingest_buffer(const char *buf, size_t len) {
    if (!buf || len == 0) return;
    char line[CTL_LINE_MAX];
    size_t line_len = 0;

    for (size_t i = 0; i < len; i++) {
        char c = buf[i];
        if (c == '\n' || c == '\r') {
            if (line_len > 0) {
                line[line_len] = '\0';
                queue_push(line);
                line_len = 0;
            }
        } else {
            if (line_len < sizeof(line) - 1) {
                line[line_len++] = c;
            }
        }
    }
    if (line_len > 0) {
        line[line_len] = '\0';
        queue_push(line);
    }
}

static bool parse_command_line(char *line, ctl_command_t *cmd) {
    if (!line || !cmd) return false;
    memset(cmd, 0, sizeof(ctl_command_t));

    /* Trim trailing newline and spaces */
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n' || line[len - 1] == ' ')) {
        line[--len] = '\0';
    }
    if (len == 0) return false;

    char *space = strchr(line, ' ');
    char *verb = line;
    char *arg = NULL;
    if (space) {
        *space = '\0';
        arg = space + 1;
        while (*arg == ' ') arg++;
    }

    if (strcasecmp(verb, "PS") == 0 && arg) {
        cmd->type = CTL_CMD_PS_SET;
        strncpy(cmd->text_arg1, arg, sizeof(cmd->text_arg1) - 1);
        cmd->text_arg1[sizeof(cmd->text_arg1) - 1] = '\0';
        return true;
    }

    if (strcasecmp(verb, "DYNAMIC_PS") == 0 && arg) {
        cmd->type = CTL_CMD_DYNAMIC_PS_SET;
        char mode_str[32] = {0};
        int interval = 2000;
        char text[128] = {0};

        if (sscanf(arg, "%31s %d %127[^\n]", mode_str, &interval, text) >= 3) {
            if (interval < 200) interval = 200;
            if (interval > 30000) interval = 30000;
            cmd->int_arg = (strcasecmp(mode_str, "scroll") == 0) ? 2 : 1;
            cmd->float_arg = (double)interval;
            strncpy(cmd->text_arg1, text, sizeof(cmd->text_arg1) - 1);
            cmd->text_arg1[sizeof(cmd->text_arg1) - 1] = '\0';
            return true;
        }
    }

    if (strcasecmp(verb, "RT") == 0 && arg) {
        cmd->type = CTL_CMD_RT_SET;
        strncpy(cmd->text_arg1, arg, sizeof(cmd->text_arg1) - 1);
        cmd->text_arg1[sizeof(cmd->text_arg1) - 1] = '\0';
        return true;
    }

    if (strcasecmp(verb, "TRACK") == 0 && arg) {
        cmd->type = CTL_CMD_RT_PLUS_SET;
        char *sep = strchr(arg, '|');
        if (!sep) sep = strstr(arg, " - ");
        if (sep) {
            *sep = '\0';
            char *title = arg;
            char *artist = sep + (sep[1] == '-' ? 3 : 1);
            while (*artist == ' ') artist++;
            size_t tlen = strlen(title);
            while (tlen > 0 && title[tlen - 1] == ' ') title[--tlen] = '\0';
            strncpy(cmd->text_arg1, title, sizeof(cmd->text_arg1) - 1);
            cmd->text_arg1[sizeof(cmd->text_arg1) - 1] = '\0';
            strncpy(cmd->text_arg2, artist, sizeof(cmd->text_arg2) - 1);
            cmd->text_arg2[sizeof(cmd->text_arg2) - 1] = '\0';
        } else {
            strncpy(cmd->text_arg1, arg, sizeof(cmd->text_arg1) - 1);
            cmd->text_arg1[sizeof(cmd->text_arg1) - 1] = '\0';
            cmd->text_arg2[0] = '\0';
        }
        return true;
    }

    if (strcasecmp(verb, "TITLE") == 0 && arg) {
        strncpy(g_cached_title, arg, sizeof(g_cached_title) - 1);
        g_cached_title[sizeof(g_cached_title) - 1] = '\0';
        cmd->type = CTL_CMD_RT_PLUS_SET;
        strncpy(cmd->text_arg1, g_cached_title, sizeof(cmd->text_arg1) - 1);
        cmd->text_arg1[sizeof(cmd->text_arg1) - 1] = '\0';
        strncpy(cmd->text_arg2, g_cached_artist, sizeof(cmd->text_arg2) - 1);
        cmd->text_arg2[sizeof(cmd->text_arg2) - 1] = '\0';
        return true;
    }

    if (strcasecmp(verb, "ARTIST") == 0 && arg) {
        strncpy(g_cached_artist, arg, sizeof(g_cached_artist) - 1);
        g_cached_artist[sizeof(g_cached_artist) - 1] = '\0';
        cmd->type = CTL_CMD_RT_PLUS_SET;
        strncpy(cmd->text_arg1, g_cached_title, sizeof(cmd->text_arg1) - 1);
        cmd->text_arg1[sizeof(cmd->text_arg1) - 1] = '\0';
        strncpy(cmd->text_arg2, g_cached_artist, sizeof(cmd->text_arg2) - 1);
        cmd->text_arg2[sizeof(cmd->text_arg2) - 1] = '\0';
        return true;
    }

    if (strcasecmp(verb, "TA") == 0 && arg) {
        cmd->type = CTL_CMD_TA_SET;
        cmd->int_arg = (strcasecmp(arg, "ON") == 0 || strcmp(arg, "1") == 0 || strcasecmp(arg, "true") == 0) ? 1 : 0;
        return true;
    }

    if (strcasecmp(verb, "TP") == 0 && arg) {
        cmd->type = CTL_CMD_TP_SET;
        cmd->int_arg = (strcasecmp(arg, "ON") == 0 || strcmp(arg, "1") == 0 || strcasecmp(arg, "true") == 0) ? 1 : 0;
        return true;
    }

    if (strcasecmp(verb, "PTY") == 0 && arg) {
        cmd->type = CTL_CMD_PTY_SET;
        int p = atoi(arg);
        if (p < 0) p = 0;
        if (p > 31) p = 31;
        cmd->int_arg = p;
        return true;
    }

    if (strcasecmp(verb, "FREQ") == 0 && arg) {
        double f = atof(arg);
        if (f >= 65.0 && f <= 108.0) {
            cmd->type = CTL_CMD_FREQ_SET;
            cmd->float_arg = f;
            return true;
        }
        return false;
    }

    if (strcasecmp(verb, "QUIT") == 0 || strcasecmp(verb, "STOP") == 0) {
        cmd->type = CTL_CMD_SHUTDOWN;
        return true;
    }

    return false;
}

bool control_poll(ctl_command_t *cmd) {
    /* First drain any already queued command */
    char line[CTL_LINE_MAX];
    while (queue_pop(line)) {
        if (parse_command_line(line, cmd)) return true;
    }

    char buf[512];

    /* Poll FIFO */
    if (g_fifo_fd >= 0) {
        ssize_t n = read(g_fifo_fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            ingest_buffer(buf, (size_t)n);
        }
    }

    /* Poll UNIX socket */
    if (g_sock_fd >= 0) {
        ssize_t n = recv(g_sock_fd, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            buf[n] = '\0';
            ingest_buffer(buf, (size_t)n);
        }
    }

    /* Check if queue has a valid command now */
    while (queue_pop(line)) {
        if (parse_command_line(line, cmd)) return true;
    }

    return false;
}

void control_cleanup(void) {
    if (g_fifo_fd >= 0) {
        close(g_fifo_fd);
        g_fifo_fd = -1;
    }
    if (strlen(g_fifo_path) > 0) {
        unlink(g_fifo_path);
        g_fifo_path[0] = '\0';
    }

    if (g_sock_fd >= 0) {
        close(g_sock_fd);
        g_sock_fd = -1;
    }
    if (strlen(g_sock_path) > 0) {
        unlink(g_sock_path);
        g_sock_path[0] = '\0';
    }
}
