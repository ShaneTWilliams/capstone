#include "dma.h"

#include "generated/values.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#define SPI_FREQ_HZ 100000U
#define SPI_CS_PIN  1

typedef enum {
    DMA_CHANNEL_SPI0_CTRL = 0,
    DMA_CHANNEL_SPI0_DATA = 1,
} dma_channel_t;

dma_channel_config spi0_ctrl_config;
dma_channel_config spi0_data_config;
static uint8_t tx_buf[] = {0x0B, 0x01, 0x00};

__attribute__((aligned(2 * sizeof(uint32_t *)))) uint32_t ctrl_block[] = {
    3U, (uint32_t)tx_buf, 3U, (uint32_t)tx_buf, 3U, (uint32_t)tx_buf, 0U, 0U,
};

static void dma_handler(void) {
    dma_hw->ints0 = 1U << DMA_CHANNEL_SPI0_DATA;
    dma_channel_configure(
        DMA_CHANNEL_SPI0_CTRL, &spi0_ctrl_config,
        &dma_hw->ch[DMA_CHANNEL_SPI0_DATA].al3_transfer_count, &ctrl_block, 2U, true
    );
}

static void init(void) {
    spi_init(spi0, SPI_FREQ_HZ);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_LSB_FIRST);

    gpio_set_function(0, GPIO_FUNC_SPI);
    gpio_set_function(2, GPIO_FUNC_SPI);
    gpio_set_function(3, GPIO_FUNC_SPI);

    gpio_init(SPI_CS_PIN);
    gpio_set_dir(SPI_CS_PIN, GPIO_OUT);
    gpio_put(SPI_CS_PIN, false);

    spi0_ctrl_config = dma_channel_get_default_config(DMA_CHANNEL_SPI0_CTRL);
    channel_config_set_transfer_data_size(&spi0_ctrl_config, DMA_SIZE_32);
    channel_config_set_read_increment(&spi0_ctrl_config, true);
    channel_config_set_write_increment(&spi0_ctrl_config, true);
    channel_config_set_ring(&spi0_ctrl_config, true, 3U);
    dma_channel_configure(
        DMA_CHANNEL_SPI0_CTRL, &spi0_ctrl_config,
        &dma_hw->ch[DMA_CHANNEL_SPI0_DATA].al3_transfer_count, &ctrl_block, 2U, false
    );

    spi0_data_config = dma_channel_get_default_config(DMA_CHANNEL_SPI0_DATA);
    channel_config_set_transfer_data_size(&spi0_data_config, DMA_SIZE_8);
    channel_config_set_dreq(&spi0_data_config, spi_get_dreq(spi0, true));
    channel_config_set_read_increment(&spi0_data_config, true);
    channel_config_set_write_increment(&spi0_data_config, false);
    channel_config_set_chain_to(&spi0_data_config, DMA_CHANNEL_SPI0_CTRL);
    channel_config_set_irq_quiet(&spi0_data_config, true);

    dma_channel_set_irq0_enabled(DMA_CHANNEL_SPI0_DATA, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    dma_channel_configure(
        DMA_CHANNEL_SPI0_DATA, &spi0_data_config, &spi_get_hw(spi0)->dr, tx_buf, sizeof(tx_buf),
        false
    );

    dma_channel_start(DMA_CHANNEL_SPI0_CTRL);
}

static void run_100ms(void) {}

module_t dma_module = {
    .name       = "dma",
    .init       = init,
    .run_1ms    = NULL,
    .run_10ms   = NULL,
    .run_100ms  = run_100ms,
    .run_1000ms = NULL,
};
