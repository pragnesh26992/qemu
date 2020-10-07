/*
 * QEMU SiFive System-on-Chip SPI register definition
 *
 * Copyright 2020 SiFive,Inc
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SIFIVE_SPI_H
#define SIFIVE_SPI_H

#include "hw/sysbus.h"

#define TYPE_SIFIVE_SPI "sifive_soc.spi"
#define SIFIVE_SPI(obj) OBJECT_CHECK(sifiveSPI, (obj), TYPE_SIFIVE_SPI)

#define R_SCKDIV            (0x00 / 4) /* Serial clock divisor */
#define R_SCKMODE           (0x04 / 4) /* Serial clock mode */
#define R_CSID              (0x10 / 4) /* Chip select ID */
#define R_CSDEF             (0x14 / 4) /* Chip select default */
#define R_CSMODE            (0x18 / 4) /* Chip select mode */
#define R_DELAY0            (0x28 / 4) /* Delay control 0 */
#define R_DELAY1            (0x2C / 4) /* Delay control 1 */
#define R_FMT               (0x40 / 4) /* Frame format */
#define R_TXDATA            (0x48 / 4) /* Tx FIFO data */
#define R_RXDATA            (0x4C / 4) /* Rx FIFO data */
#define R_TXMARK            (0x50 / 4) /* Tx FIFO watermark */
#define R_RXMARK            (0x54 / 4) /* Rx FIFO watermark */
#define R_FCTRL             (0x60 / 4) /* SPI flash interface control */
#define R_FFMT              (0x64 / 4) /* SPI flash instruction format */
#define R_IE                (0x70 / 4) /* Interrupt Enable Register */
#define R_IP                (0x74 / 4) /* Interrupt Pendings Register */
#define R_MAX               (0x78 / 4)

/* ie and ip bits */
#define SIFIVE_SPI_IP_TXWM               BIT(0)
#define SIFIVE_SPI_IP_RXWM               BIT(1)

#define SIFIVE_SPI_RXDATA_DATA_MASK      0xffU

typedef struct sifiveSPI {
    SysBusDevice parent_obj;

    MemoryRegion mmio;

    qemu_irq irq;
    int irqline;

    uint8_t num_cs;
    qemu_irq *cs_lines;

    SSIBus *ssi;
    /* The FIFO head points to the next empty entry.  */
    int tx_fifo_head;
    int rx_fifo_head;
    int tx_fifo_len;
    int rx_fifo_len;
    uint16_t tx_fifo[8];
    uint16_t rx_fifo[8];

    uint32_t regs[R_MAX];
} sifiveSPI;

#endif
