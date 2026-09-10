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
        return true;
    }

    if (strcasecmp(verb, "DYNAMIC_PS") == 0 && arg) {
        cmd->type = CTL_CMD_DYNAMIC_PS_SET;
        char mode_str[32] = {0};
        int interval = 2000;
        char text[128] = {0};

        if (sscanf(arg, "%31s %d %127[^\n]", mode_str, &interval, text) >= 3) {
            cmd->int_arg = (strcasecmp(mode_str, "scroll") == 0) ? 2 : 1;
            cmd->float_arg = (double)interval;
            strncpy(cmd->text_arg1, text, sizeof(cmd->text_arg1) - 1);
            return true;
        }
    }

    if (strcasecmp(verb, "RT") == 0 && arg) {
        cmd->type = CTL_CMD_RT_SET;
        strncpy(cmd->text_arg1, arg, sizeof(cmd->text_arg1) - 1);
        return true;
    }

    if (strcasecmp(verb, "TITLE") == 0 && arg) {
        strncpy(g_cached_title, arg, sizeof(g_cached_title) - 1);
        cmd->type = CTL_CMD_RT_PLUS_SET;
        strncpy(cmd->text_arg1, g_cached_title, sizeof(cmd->text_arg1) - 1);
        strncpy(cmd->text_arg2, g_cached_artist, sizeof(cmd->text_arg2) - 1);
        return true;
    }

    if (strcasecmp(verb, "ARTIST") == 0 && arg) {
        strncpy(g_cached_artist, arg, sizeof(g_cached_artist) - 1);
        cmd->type = CTL_CMD_RT_PLUS_SET;
        strncpy(cmd->text_arg1, g_cached_title, sizeof(cmd->text_arg1) - 1);
        strncpy(cmd->text_arg2, g_cached_artist, sizeof(cmd->text_arg2) - 1);
        return true;
    }

    if (strcasecmp(verb, "TA") == 0 && arg) {
        cmd->type = CTL_CMD_TA_SET;
        cmd->int_arg = (strcasecmp(arg, "ON") == 0 || strcmp(arg, "1") == 0) ? 1 : 0;
        return true;
    }

    if (strcasecmp(verb, "TP") == 0 && arg) {
        cmd->type = CTL_CMD_TP_SET;
        cmd->int_arg = (strcasecmp(arg, "ON") == 0 || strcmp(arg, "1") == 0) ? 1 : 0;
        return true;
    }

    if (strcasecmp(verb, "PTY") == 0 && arg) {
        cmd->type = CTL_CMD_PTY_SET;
        cmd->int_arg = atoi(arg);
        return true;
    }

    if (strcasecmp(verb, "FREQ") == 0 && arg) {
        cmd->type = CTL_CMD_FREQ_SET;
        cmd->float_arg = atof(arg);
        return true;
    }

    if (strcasecmp(verb, "QUIT") == 0 || strcasecmp(verb, "STOP") == 0) {
        cmd->type = CTL_CMD_SHUTDOWN;
        return true;
    }

    return false;
}

bool control_poll(ctl_command_t *cmd) {
    char buf[CTL_LINE_MAX] = {0};

    /* Check FIFO */
    if (g_fifo_fd >= 0) {
        ssize_t n = read(g_fifo_fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            if (parse_command_line(buf, cmd)) return true;
        }
    }

    /* Check UNIX socket */
    if (g_sock_fd >= 0) {
        ssize_t n = recv(g_sock_fd, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            buf[n] = '\0';
            if (parse_command_line(buf, cmd)) return true;
        }
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
