/*
 * PiFmRds-ng - Next-Generation FM/RDS transmitter
 * Modernized for 2026 standards
 *
 * Hardware Abstraction Layer for Raspberry Pi Direct DMA/GPCLK0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include "hw_rpi.h"
#include "mailbox.h"

#define NUM_SAMPLES     50000
#define NUM_CBS         (NUM_SAMPLES * 2)

#define BCM2708_DMA_NO_WIDE_BURSTS  (1 << 26)
#define BCM2708_DMA_WAIT_RESP       (1 << 3)
#define BCM2708_DMA_D_DREQ          (1 << 6)
#define BCM2708_DMA_PER_MAP(x)      ((x) << 16)
#define BCM2708_DMA_END             (1 << 1)
#define BCM2708_DMA_RESET           (1 << 31)
#define BCM2708_DMA_INT             (1 << 2)

#define DMA_CS          (0x00 / 4)
#define DMA_CONBLK_AD   (0x04 / 4)
#define DMA_DEBUG       (0x20 / 4)

#define DMA_BASE_OFFSET   0x00007000
#define DMA_LEN           0x24
#define PWM_BASE_OFFSET   0x0020C000
#define PWM_LEN           0x28
#define CLK_BASE_OFFSET   0x00101000
#define CLK_LEN           0xA8
#define GPIO_BASE_OFFSET  0x00200000
#define GPIO_LEN          0x100

#define PWM_CTL         (0x00 / 4)
#define PWM_DMAC        (0x08 / 4)
#define PWM_RNG1        (0x10 / 4)
#define PWM_FIFO        (0x18 / 4)

#define GPCLK_CNTL      (0x70 / 4)
#define GPCLK_DIV       (0x74 / 4)
#define PWMCLK_CNTL     40
#define PWMCLK_DIV      41

#define CM_GP0DIV       (0x7E101074)

#define PWMCTL_MODE1    (1 << 1)
#define PWMCTL_PWEN1    (1 << 0)
#define PWMCTL_CLRF     (1 << 6)
#define PWMCTL_USEF1    (1 << 5)
#define PWMDMAC_ENAB    (1 << 31)
#define PWMDMAC_THRSHLD ((15 << 8) | (15 << 0))

#define GPFSEL0         (0x00 / 4)

typedef struct {
    uint32_t info, src, dst, length;
    uint32_t stride, next, pad[2];
} dma_cb_t;

struct control_data_s {
    dma_cb_t cb[NUM_CBS];
    uint32_t sample[NUM_SAMPLES];
};

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12
#define NUM_PAGES ((sizeof(struct control_data_s) + PAGE_SIZE - 1) >> PAGE_SHIFT)

static volatile uint32_t *g_pwm_reg = NULL;
static volatile uint32_t *g_clk_reg = NULL;
static volatile uint32_t *g_dma_reg = NULL;
static volatile uint32_t *g_gpio_reg = NULL;

static struct {
    int handle;
    uint32_t mem_ref;
    uint32_t bus_addr;
    uint8_t *virt_addr;
} g_mbox = {0};

static struct control_data_s *g_ctl = NULL;
static uint32_t g_freq_ctl = 0;
static rpi_hw_info_t g_hw_info;
static bool g_hw_active = false;

static void udelay(int us) {
    struct timespec ts = {0, (long)us * 1000L};
    nanosleep(&ts, NULL);
}

#define BUS_TO_PHYS(x) ((x) & ~0xC0000000)

static size_t mem_virt_to_phys(void *virt) {
    size_t offset = (size_t)virt - (size_t)g_mbox.virt_addr;
    return g_mbox.bus_addr + offset;
}

static size_t mem_phys_to_virt(size_t phys) {
    return (size_t)(phys - g_mbox.bus_addr + g_mbox.virt_addr);
}

int rpi_hw_detect(rpi_hw_info_t *info) {
    if (!info) return -1;
    memset(info, 0, sizeof(rpi_hw_info_t));

#ifdef __linux__
    FILE *f = fopen("/proc/device-tree/model", "r");
    if (!f) {
        f = fopen("/proc/cpuinfo", "r");
    }

    if (f) {
        char buf[256] = {0};
        if (fgets(buf, sizeof(buf) - 1, f)) {
            strncpy(info->model_name, buf, sizeof(info->model_name) - 1);
        }
        fclose(f);
    }
#else
    strncpy(info->model_name, "Non-Linux Host", sizeof(info->model_name) - 1);
#endif

    if (strstr(info->model_name, "Raspberry Pi 5") != NULL ||
        strstr(info->model_name, "BCM2712") != NULL) {
        info->soc = RPI_SOC_BCM2712;
        info->direct_rf_supported = false;
        snprintf(info->compatibility_note, sizeof(info->compatibility_note),
                 "Raspberry Pi 5 uses the RP1 southbridge over PCIe. Direct GPIO RF synthesis is "
                 "not supported on the 40-pin header. Use MPX/SDR mode (HackRF, FL2k, or WAV output).");
        return 0;
    }

    if (strstr(info->model_name, "Raspberry Pi 4") != NULL ||
        strstr(info->model_name, "Pi 400") != NULL ||
        strstr(info->model_name, "Compute Module 4") != NULL) {
        info->soc = RPI_SOC_BCM2711;
        info->periph_base = 0xFE000000;
        info->dram_phys_base = 0xC0000000;
        info->mem_flag = 0x04;
        info->pll_freq = 750000000.0;
        info->direct_rf_supported = true;
        snprintf(info->compatibility_note, sizeof(info->compatibility_note), "Raspberry Pi 4 (BCM2711) - Direct RF Supported.");
        return 0;
    }

    if (strstr(info->model_name, "Raspberry Pi 3") != NULL ||
        strstr(info->model_name, "Zero 2") != NULL ||
        strstr(info->model_name, "Compute Module 3") != NULL) {
        info->soc = RPI_SOC_BCM2837;
        info->periph_base = 0x3F000000;
        info->dram_phys_base = 0xC0000000;
        info->mem_flag = 0x04;
        info->pll_freq = 500000000.0;
        info->direct_rf_supported = true;
        snprintf(info->compatibility_note, sizeof(info->compatibility_note), "Raspberry Pi 3 / Zero 2W (BCM2837) - Direct RF Supported.");
        return 0;
    }

    if (strstr(info->model_name, "Raspberry Pi 2") != NULL) {
        info->soc = RPI_SOC_BCM2836;
        info->periph_base = 0x3F000000;
        info->dram_phys_base = 0xC0000000;
        info->mem_flag = 0x04;
        info->pll_freq = 500000000.0;
        info->direct_rf_supported = true;
        snprintf(info->compatibility_note, sizeof(info->compatibility_note), "Raspberry Pi 2 (BCM2836) - Direct RF Supported.");
        return 0;
    }

    if (strstr(info->model_name, "Raspberry Pi") != NULL ||
        strstr(info->model_name, "BCM2835") != NULL) {
        info->soc = RPI_SOC_BCM2835;
        info->periph_base = 0x20000000;
        info->dram_phys_base = 0x40000000;
        info->mem_flag = 0x0C;
        info->pll_freq = 500000000.0;
        info->direct_rf_supported = true;
        snprintf(info->compatibility_note, sizeof(info->compatibility_note), "Raspberry Pi 1 / Zero (BCM2835) - Direct RF Supported.");
        return 0;
    }

    info->soc = RPI_SOC_UNKNOWN;
    info->direct_rf_supported = false;
    snprintf(info->compatibility_note, sizeof(info->compatibility_note),
             "Non-Raspberry Pi hardware detected. Direct RF unavailable; use universal WAV/MPX export.");
    return 0;
}

int rpi_hw_init(const rpi_hw_info_t *info, uint32_t carrier_freq_hz, float ppm) {
    if (!info || !info->direct_rf_supported) {
        fprintf(stderr, "[HW] Error: Hardware does not support direct GPIO RF synthesis.\n");
        return -1;
    }

    g_hw_info = *info;
    uint32_t base = g_hw_info.periph_base;

    g_dma_reg  = (volatile uint32_t *)mapmem(base + DMA_BASE_OFFSET, DMA_LEN);
    g_pwm_reg  = (volatile uint32_t *)mapmem(base + PWM_BASE_OFFSET, PWM_LEN);
    g_clk_reg  = (volatile uint32_t *)mapmem(base + CLK_BASE_OFFSET, CLK_LEN);
    g_gpio_reg = (volatile uint32_t *)mapmem(base + GPIO_BASE_OFFSET, GPIO_LEN);

    if (!g_dma_reg || !g_pwm_reg || !g_clk_reg || !g_gpio_reg) {
        fprintf(stderr, "[HW] Error: Failed to map SoC peripheral registers via /dev/mem or /dev/gpiomem.\n");
        rpi_hw_shutdown();
        return -1;
    }

    /* Open VideoCore mailbox to allocate contiguous uncached physical memory */
    g_mbox.handle = mbox_open();
    if (g_mbox.handle < 0) {
        fprintf(stderr, "[HW] Error: Failed to open VideoCore mailbox (/dev/vcio).\n");
        rpi_hw_shutdown();
        return -1;
    }

    size_t alloc_size = NUM_PAGES * 4096;
    g_mbox.mem_ref = mem_alloc(g_mbox.handle, (uint32_t)alloc_size, 4096, g_hw_info.mem_flag);
    if (!g_mbox.mem_ref) {
        fprintf(stderr, "[HW] Error: mem_alloc failed via mailbox.\n");
        rpi_hw_shutdown();
        return -1;
    }

    g_mbox.bus_addr = mem_lock(g_mbox.handle, g_mbox.mem_ref);
    if (!g_mbox.bus_addr) {
        fprintf(stderr, "[HW] Error: mem_lock failed via mailbox.\n");
        rpi_hw_shutdown();
        return -1;
    }

    g_mbox.virt_addr = (uint8_t *)mapmem(BUS_TO_PHYS(g_mbox.bus_addr), alloc_size);
    if (!g_mbox.virt_addr) {
        fprintf(stderr, "[HW] Error: mapmem failed for mailbox buffer.\n");
        rpi_hw_shutdown();
        return -1;
    }

    /* Configure GPIO 4 to ALT FUNC 0 (GPCLK0) */
    g_gpio_reg[GPFSEL0] = (g_gpio_reg[GPFSEL0] & ~(7 << 12)) | (4 << 12);

    /* Program GPCLK0 with MASH setting 1 */
    g_clk_reg[GPCLK_CNTL] = (0x5A << 24) | 6;
    udelay(100);
    g_clk_reg[GPCLK_CNTL] = (0x5A << 24) | (1 << 9) | (1 << 4) | 6;

    g_ctl = (struct control_data_s *)g_mbox.virt_addr;
    dma_cb_t *cbp = g_ctl->cb;
    uint32_t phys_sample_dst = CM_GP0DIV;
    uint32_t phys_pwm_fifo_addr = 0x7E000000 + PWM_BASE_OFFSET + 0x18;

    /* Calculate carrier frequency control word: (PLLFREQ / carrier_freq) * 4096 */
    g_freq_ctl = (uint32_t)(((double)g_hw_info.pll_freq / carrier_freq_hz) * 4096.0);

    for (int i = 0; i < NUM_SAMPLES; i++) {
        g_ctl->sample[i] = (0x5A << 24) | g_freq_ctl;

        /* Frequency sample write CB */
        cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP;
        cbp->src = (uint32_t)mem_virt_to_phys(&g_ctl->sample[i]);
        cbp->dst = phys_sample_dst;
        cbp->length = 4;
        cbp->stride = 0;
        cbp->next = (uint32_t)mem_virt_to_phys(cbp + 1);
        cbp++;

        /* PWM FIFO delay pacing CB */
        cbp->info = BCM2708_DMA_NO_WIDE_BURSTS | BCM2708_DMA_WAIT_RESP | BCM2708_DMA_D_DREQ | BCM2708_DMA_PER_MAP(5);
        cbp->src = (uint32_t)mem_virt_to_phys(g_mbox.virt_addr);
        cbp->dst = phys_pwm_fifo_addr;
        cbp->length = 4;
        cbp->stride = 0;
        cbp->next = (uint32_t)mem_virt_to_phys(cbp + 1);
        cbp++;
    }
    cbp--;
    cbp->next = (uint32_t)mem_virt_to_phys(g_mbox.virt_addr);

    /* Clock divider pacing setup (228 kHz update rate) */
    double divider = g_hw_info.pll_freq / (2000.0 * 228.0 * (1.0 + ppm / 1.0e6));
    uint32_t idivider = (uint32_t)divider;
    uint32_t fdivider = (uint32_t)((divider - idivider) * 4096.0);

    g_pwm_reg[PWM_CTL] = 0;
    udelay(10);
    g_clk_reg[PWMCLK_CNTL] = 0x5A000006;
    udelay(100);
    g_clk_reg[PWMCLK_DIV] = 0x5A000000 | (idivider << 12) | fdivider;
    udelay(100);
    g_clk_reg[PWMCLK_CNTL] = 0x5A000216;
    udelay(100);

    g_pwm_reg[PWM_RNG1] = 2;
    udelay(10);
    g_pwm_reg[PWM_DMAC] = PWMDMAC_ENAB | PWMDMAC_THRSHLD;
    udelay(10);
    g_pwm_reg[PWM_CTL] = PWMCTL_CLRF;
    udelay(10);
    g_pwm_reg[PWM_CTL] = PWMCTL_USEF1 | PWMCTL_PWEN1;
    udelay(10);

    /* Start DMA Engine */
    g_dma_reg[DMA_CS] = BCM2708_DMA_RESET;
    udelay(10);
    g_dma_reg[DMA_CS] = BCM2708_DMA_INT | BCM2708_DMA_END;
    g_dma_reg[DMA_CONBLK_AD] = (uint32_t)mem_virt_to_phys(g_ctl->cb);
    g_dma_reg[DMA_DEBUG] = 7;
    g_dma_reg[DMA_CS] = 0x10880001; /* Start DMA */

    g_hw_active = true;
    printf("[HW] DMA Engine started successfully. Carrier: %.2f MHz.\n", carrier_freq_hz / 1.0e6);
    return 0;
}

int rpi_hw_write_samples(const float *samples, size_t count, float deviation_khz) {
    if (!g_hw_active || !g_ctl || !g_dma_reg) return -1;

    static int last_sample = 0;
    size_t cur_cb = mem_phys_to_virt(g_dma_reg[DMA_CONBLK_AD]);
    int this_sample = (int)((cur_cb - (size_t)g_mbox.virt_addr) / (sizeof(dma_cb_t) * 2));
    int free_slots = this_sample - last_sample;
    if (free_slots < 0) free_slots += NUM_SAMPLES;

    size_t written = 0;
    while (written < count && free_slots > 0) {
        float dval = samples[written] * (deviation_khz / 10.0f);
        int intval = (int)floor(dval);

        g_ctl->sample[last_sample++] = (0x5A << 24 | g_freq_ctl) + intval;
        if (last_sample >= NUM_SAMPLES) last_sample = 0;

        written++;
        free_slots--;
    }

    return (int)written;
}

void rpi_hw_shutdown(void) {
    if (!g_hw_active) return;
    g_hw_active = false;

    /* Stop clock generation on GPIO4 and restore to standard output */
    if (g_gpio_reg && g_clk_reg) {
        g_gpio_reg[GPFSEL0] = (g_gpio_reg[GPFSEL0] & ~(7 << 12)) | (1 << 12);
        g_clk_reg[GPCLK_CNTL] = 0x5A;
    }

    /* Reset and halt DMA */
    if (g_dma_reg) {
        g_dma_reg[DMA_CS] = BCM2708_DMA_RESET;
        udelay(10);
    }

    /* Release VideoCore mailbox memory */
    if (g_mbox.virt_addr != NULL) {
        unmapmem(g_mbox.virt_addr, NUM_PAGES * 4096);
        mem_unlock(g_mbox.handle, g_mbox.mem_ref);
        mem_free(g_mbox.handle, g_mbox.mem_ref);
        mbox_close(g_mbox.handle);
        memset(&g_mbox, 0, sizeof(g_mbox));
    }

    printf("[HW] DMA engine halted and RF carrier safely killed.\n");
}
