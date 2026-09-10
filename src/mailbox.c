/*
 * PiFmRds-ng - VideoCore Mailbox Interface
 * Modernized for 64-bit aarch64 and modern Linux 6.x kernels
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include "mailbox.h"

#ifdef __linux__
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#define PAGE_SIZE 4096

void *mapmem(uintptr_t base, size_t size) {
    int mem_fd;
    uintptr_t offset = base % PAGE_SIZE;
    uintptr_t aligned_base = base - offset;

    /* First try /dev/mem */
    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        /* Fallback: try /dev/gpiomem */
        mem_fd = open("/dev/gpiomem", O_RDWR | O_SYNC);
    }

    if (mem_fd < 0) {
        fprintf(stderr, "[Mailbox] Error: Cannot open /dev/mem or /dev/gpiomem.\n"
                        "  -> Run with 'sudo' or check kernel boot parameter 'iomem=relaxed'\n");
        return NULL;
    }

    void *mem = mmap(NULL, size + offset, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, (off_t)aligned_base);
    close(mem_fd);

    if (mem == MAP_FAILED) {
        fprintf(stderr, "[Mailbox] Error: mmap failed for physical address 0x%lx\n", (unsigned long)aligned_base);
        return NULL;
    }

    return (char *)mem + offset;
}

void *unmapmem(void *addr, size_t size) {
    if (addr) {
        uintptr_t base = (uintptr_t)addr;
        uintptr_t offset = base % PAGE_SIZE;
        munmap((void *)(base - offset), size + offset);
    }
    return NULL;
}

static int mbox_property(int file_desc, void *buf) {
    int ret_val = ioctl(file_desc, IOCTL_MBOX_PROPERTY, buf);
    if (ret_val < 0) {
        perror("[Mailbox] ioctl IOCTL_MBOX_PROPERTY failed");
    }
    return ret_val;
}

uint32_t mem_alloc(int file_desc, uint32_t size, uint32_t align, uint32_t flags) {
    int i = 0;
    uint32_t p[32];
    p[i++] = 0;             // size
    p[i++] = 0x00000000;    // process request

    p[i++] = 0x3000c;       // (the tag id)
    p[i++] = 12;            // (size of the buffer)
    p[i++] = 12;            // (size of the data)
    p[i++] = size;          // req: size
    p[i++] = align;         // req: align
    p[i++] = flags;         // req: flags

    p[i++] = 0x00000000;    // end tag
    p[0] = i * sizeof(uint32_t); // actual size

    if (mbox_property(file_desc, p) < 0) return 0;
    return p[5];
}

uint32_t mem_free(int file_desc, uint32_t handle) {
    int i = 0;
    uint32_t p[32];
    p[i++] = 0;
    p[i++] = 0x00000000;

    p[i++] = 0x3000f;
    p[i++] = 4;
    p[i++] = 4;
    p[i++] = handle;

    p[i++] = 0x00000000;
    p[0] = i * sizeof(uint32_t);

    if (mbox_property(file_desc, p) < 0) return 0;
    return p[5];
}

uint32_t mem_lock(int file_desc, uint32_t handle) {
    int i = 0;
    uint32_t p[32];
    p[i++] = 0;
    p[i++] = 0x00000000;

    p[i++] = 0x3000d;
    p[i++] = 4;
    p[i++] = 4;
    p[i++] = handle;

    p[i++] = 0x00000000;
    p[0] = i * sizeof(uint32_t);

    if (mbox_property(file_desc, p) < 0) return 0;
    return p[5];
}

uint32_t mem_unlock(int file_desc, uint32_t handle) {
    int i = 0;
    uint32_t p[32];
    p[i++] = 0;
    p[i++] = 0x00000000;

    p[i++] = 0x3000e;
    p[i++] = 4;
    p[i++] = 4;
    p[i++] = handle;

    p[i++] = 0x00000000;
    p[0] = i * sizeof(uint32_t);

    if (mbox_property(file_desc, p) < 0) return 0;
    return p[5];
}

int mbox_open(void) {
    int file_desc = open(DEVICE_FILE_NAME, 0);
    if (file_desc < 0) {
        unlink(LOCAL_DEVICE_FILE_NAME);
        if (mknod(LOCAL_DEVICE_FILE_NAME, S_IFCHR | 0600, makedev(MAJOR_NUM_A, 0)) >= 0) {
            file_desc = open(LOCAL_DEVICE_FILE_NAME, 0);
        }
    }
    if (file_desc < 0) {
        unlink(LOCAL_DEVICE_FILE_NAME);
        if (mknod(LOCAL_DEVICE_FILE_NAME, S_IFCHR | 0600, makedev(MAJOR_NUM_B, 0)) >= 0) {
            file_desc = open(LOCAL_DEVICE_FILE_NAME, 0);
        }
    }
    return file_desc;
}

void mbox_close(int file_desc) {
    if (file_desc >= 0) {
        close(file_desc);
    }
}

#else
/* Non-Linux fallback stubs (for building tests/tools on macOS or other platforms) */
void *mapmem(uintptr_t base, size_t size) { (void)base; (void)size; return NULL; }
void *unmapmem(void *addr, size_t size) { (void)addr; (void)size; return NULL; }
int mbox_open(void) { return -1; }
void mbox_close(int file_desc) { (void)file_desc; }
uint32_t mem_alloc(int file_desc, uint32_t size, uint32_t align, uint32_t flags) {
    (void)file_desc; (void)size; (void)align; (void)flags; return 0;
}
uint32_t mem_free(int file_desc, uint32_t handle) { (void)file_desc; (void)handle; return 0; }
uint32_t mem_lock(int file_desc, uint32_t handle) { (void)file_desc; (void)handle; return 0; }
uint32_t mem_unlock(int file_desc, uint32_t handle) { (void)file_desc; (void)handle; return 0; }
#endif
