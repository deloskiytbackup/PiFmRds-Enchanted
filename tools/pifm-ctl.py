#!/usr/bin/env python3
"""
PiFmRds-ng Control Utility (2026 Edition)
Sends commands to running pi_fm_rds instance via UNIX socket or FIFO pipe.
Supports manual updates or automatic metadata sync from players (MPD, Spotify, PipeWire).
"""

import argparse
import os
import socket
import sys
import time

DEFAULT_FIFO = "/tmp/pifm_ctl"
DEFAULT_SOCK = "/tmp/pifm_sock"

def send_command(cmd: str, fifo_path: str = DEFAULT_FIFO, sock_path: str = DEFAULT_SOCK) -> bool:
    sent = False
    cmd_bytes = (cmd.strip() + "\n").encode("utf-8")

    # Try UNIX domain socket first
    if os.path.exists(sock_path):
        try:
            with socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM) as s:
                s.connect(sock_path)
                s.send(cmd_bytes)
                print(f"[OK] Sent to socket ({sock_path}): {cmd.strip()}")
                return True
        except Exception as e:
            pass

    # Fallback: FIFO pipe
    if os.path.exists(fifo_path):
        try:
            fd = os.open(fifo_path, os.O_WRONLY | os.O_NONBLOCK)
            with os.fdopen(fd, "w") as f:
                f.write(cmd.strip() + "\n")
            print(f"[OK] Sent to FIFO ({fifo_path}): {cmd.strip()}")
            return True
        except Exception as e:
            print(f"[WARN] Failed writing to FIFO {fifo_path}: {e}")

    print(f"[ERROR] Could not connect to {sock_path} or {fifo_path}. Is pi_fm_rds running?")
    return False

def main():
    parser = argparse.ArgumentParser(description="PiFmRds-ng 2026 Dynamic RDS Controller")
    parser.add_argument("--fifo", default=DEFAULT_FIFO, help="Path to control FIFO pipe")
    parser.add_argument("--sock", default=DEFAULT_SOCK, help="Path to control UNIX socket")
    
    parser.add_argument("--ps", help="Set Station Name (8 characters, e.g. 'MYRADIO')")
    parser.add_argument("--dynamic-ps", help="Set long Dynamic PS text")
    parser.add_argument("--ps-mode", choices=["page", "scroll"], default="page", help="Dynamic PS mode")
    parser.add_argument("--ps-rate", type=int, default=2000, help="Dynamic PS interval in ms")
    
    parser.add_argument("--rt", help="Set RadioText (up to 64 characters)")
    parser.add_argument("--title", help="Set Track Title (updates RT and RT+ tags)")
    parser.add_argument("--artist", help="Set Track Artist (updates RT and RT+ tags)")
    
    parser.add_argument("--ta", choices=["on", "off"], help="Toggle Traffic Announcement (TA)")
    parser.add_argument("--tp", choices=["on", "off"], help="Toggle Traffic Programme (TP)")
    parser.add_argument("--pty", type=int, help="Set Program Type (0-31)")
    parser.add_argument("--stop", action="store_true", help="Send shutdown command to pi_fm_rds")

    args = parser.parse_args()

    commands = []
    if args.ps:
        commands.append(f"PS {args.ps}")
    if args.dynamic_ps:
        commands.append(f"DYNAMIC_PS {args.ps_mode} {args.ps_rate} {args.dynamic_ps}")
    if args.rt:
        commands.append(f"RT {args.rt}")
    if args.title:
        commands.append(f"TITLE {args.title}")
    if args.artist:
        commands.append(f"ARTIST {args.artist}")
    if args.ta:
        commands.append(f"TA {args.ta.upper()}")
    if args.tp:
        commands.append(f"TP {args.tp.upper()}")
    if args.pty is not None:
        commands.append(f"PTY {args.pty}")
    if args.stop:
        commands.append("QUIT")

    if not commands:
        parser.print_help()
        sys.exit(1)

    success = True
    for cmd in commands:
        if not send_command(cmd, args.fifo, args.sock):
            success = False

    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
