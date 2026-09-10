/*
 * PiFmRds-ng - Next-Generation FM/RDS transmitter
 * Modernized for 2026 standards
 *
 * Hardware Abstraction Layer for Raspberry Pi SoC & Direct DMA/GPCLK0
 */

#ifndef HW_RPI_H
#define HW_RPI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RPI_SOC_UNKNOWN = 0,
    RPI_SOC_BCM2835 = 1, /* Pi 1, Pi Zero */
    RPI_SOC_BCM2836 = 2, /* Pi 2 early revs */
    RPI_SOC_BCM2837 = 3, /* Pi 2 v1.2, Pi 3, Zero 2W */
    RPI_SOC_BCM2711 = 4, /* Pi 4, 400, CM4 */
    RPI_SOC_BCM2712 = 5  /* Pi 5 (RP1 PCIe southbridge - unsupported for direct GPIO RF) */
} rpi_soc_type_t;

typedef struct {
    rpi_soc_type_t soc;
    char model_name[128];
    uint32_t periph_base;
    uint32_t dram_phys_base;
    uint32_t mem_flag;
    double pll_freq;
    bool direct_rf_supported;
    char compatibility_note[256];
} rpi_hw_info_t;

/* Detect hardware configuration at runtime */
int rpi_hw_detect(rpi_hw_info_t *info);

/* Initialize hardware peripherals, allocate mailbox DMA buffers, setup GPCLK0 on GPIO4 */
int rpi_hw_init(const rpi_hw_info_t *info, uint32_t carrier_freq_hz, float ppm);

/* Push MPX audio samples to circular DMA buffer.
   deviation_khz is typically 75.0 for standard FM broadcast. */
int rpi_hw_write_samples(const float *samples, size_t count, float deviation_khz);

/* Stop DMA, deactivate GPCLK0 clock, restore GPIO4, free physical memory */
void rpi_hw_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* HW_RPI_H */
