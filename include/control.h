/*
 * PiFmRds-ng - Next-Generation FM/RDS transmitter
 * Modernized for 2026 standards
 *
 * Real-time Control Server (UNIX Socket & FIFO Pipe)
 */

#ifndef CONTROL_H
#define CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CTL_CMD_NONE = 0,
    CTL_CMD_PS_SET,
    CTL_CMD_DYNAMIC_PS_SET,
    CTL_CMD_RT_SET,
    CTL_CMD_RT_PLUS_SET,
    CTL_CMD_TA_SET,
    CTL_CMD_TP_SET,
    CTL_CMD_PTY_SET,
    CTL_CMD_FREQ_SET,
    CTL_CMD_SHUTDOWN
} ctl_cmd_type_t;

typedef struct {
    ctl_cmd_type_t type;
    char text_arg1[128];
    char text_arg2[128];
    int int_arg;
    double float_arg;
} ctl_command_t;

/* Open control listener on FIFO path and/or UNIX domain socket path */
int control_init(const char *fifo_path, const char *socket_path);

/* Non-blocking poll for incoming commands. Returns true if a command was received. */
bool control_poll(ctl_command_t *cmd);

/* Close sockets and remove FIFO */
void control_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_H */
