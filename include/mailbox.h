/*
 * PiFmRds-ng - VideoCore Mailbox Interface (Linux/Raspberry Pi)
 * Modernized for 2026 / 64-bit kernels (Linux 5.x / 6.x)
 */

#ifndef MAILBOX_H
#define MAILBOX_H

#include <stdint.h>
#include <stddef.h>

#ifdef __linux__
#include <linux/ioctl.h>
#define MAJOR_NUM_A 249
#define MAJOR_NUM_B 100
#define IOCTL_MBOX_PROPERTY _IOWR(MAJOR_NUM_B, 0, char *)
#define DEVICE_FILE_NAME "/dev/vcio"
#endif

#ifdef __cplusplus
extern "C" {
#endif

int mbox_open(void);
void mbox_close(int file_desc);

uint32_t mem_alloc(int file_desc, uint32_t size, uint32_t align, uint32_t flags);
uint32_t mem_free(int file_desc, uint32_t handle);
uint32_t mem_lock(int file_desc, uint32_t handle);
uint32_t mem_unlock(int file_desc, uint32_t handle);

void *mapmem(uintptr_t base, size_t size);
void *unmapmem(void *addr, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* MAILBOX_H */
