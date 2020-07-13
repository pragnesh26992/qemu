/*
 * QEMU model of the Xilinx SPI Controller
 *
 * Copyright (C) 2010 Edgar E. Iglesias.
 * Copyright (C) 2012 Peter A. G. Crosthwaite <peter.crosthwaite@petalogix.com>
 * Copyright (C) 2012 PetaLogix
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "hw/riscv/sifive_u_spi.h"

#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/ssi/ssi.h"

#ifdef SIFIVE_SPI_ERR_DEBUG
#define DB_PRINT(...) do { \
    fprintf(stderr,  ": %s: ", __func__); \
    fprintf(stderr, ## __VA_ARGS__); \
    } while (0)
#else
    #define DB_PRINT(...)
#endif

#if 0
static void xlx_spi_update_cs(sifiveSPI *s)
{
    int i;

    for (i = 0; i < s->num_cs; ++i) {
        qemu_set_irq(s->cs_lines[i], !(~s->regs[R_SPISSR] & 1 << i));
    }
}

static void xlx_spi_update_irq(sifiveSPI *s)
{
    uint32_t pending;

    s->regs[R_IPISR] |=
            (!fifo8_is_empty(&s->rx_fifo) ? IRQ_DRR_NOT_EMPTY : 0) |
            (fifo8_is_full(&s->rx_fifo) ? IRQ_DRR_FULL : 0);

    pending = s->regs[R_IPISR] & s->regs[R_IPIER];

    pending = pending && (s->regs[R_DGIER] & R_DGIER_IE);
    pending = !!pending;

    /* This call lies right in the data paths so don't call the
       irq chain unless things really changed.  */
    if (pending != s->irqline) {
        s->irqline = pending;
        DB_PRINT("irq_change of state %d ISR:%x IER:%X\n",
                    pending, s->regs[R_IPISR], s->regs[R_IPIER]);
        qemu_set_irq(s->irq, pending);
    }

}
#endif
static void sifive_spi_do_reset(sifiveSPI *s)
{
    memset(s->regs, 0, sizeof s->regs);

    s->regs[R_TXMARK] = 1;
}

static void sifive_spi_reset(DeviceState *d)
{
    sifive_spi_do_reset(SIFIVE_SPI(d));
}

static void sifive_spi_transfer(sifiveSPI *s)
{
    DB_PRINT("Data to send: 0x%x\n", s->regs[R_TXDATA]);

    s->regs[R_RXDATA] = ssi_transfer(s->ssi, s->regs[R_TXDATA]);

    DB_PRINT("Data received: 0x%x\n", s->regs[R_RXDATA]);
}

static uint64_t
spi_read(void *opaque, hwaddr addr, unsigned int size)
{
    sifiveSPI *s = opaque;
    uint32_t r = 0;

    addr >>= 2;
    switch (addr) {
    case R_RXDATA:
        sifive_spi_transfer(s);
        r = s->regs[R_RXDATA];
        break;
    default:
        if (addr < ARRAY_SIZE(s->regs)) {
            r = s->regs[addr];
        }
        break;
    }

    DB_PRINT("addr=" TARGET_FMT_plx " = %x\n", addr * 4, r);
    return r;
}

static void
spi_write(void *opaque, hwaddr addr,
            uint64_t val64, unsigned int size)
{
    sifiveSPI *s = opaque;
    uint32_t value = val64;

    DB_PRINT("addr=" TARGET_FMT_plx " = %x\n", addr, value);
    addr >>= 2;
    switch (addr) {
    case R_TXDATA:
        sifive_spi_transfer(s);
    default:
        if (addr < ARRAY_SIZE(s->regs)) {
            s->regs[addr] = value;
        }
        break;
    }
}

static const MemoryRegionOps spi_ops = {
    .read = spi_read,
    .write = spi_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4
    }
};

static void sifive_spi_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    sifiveSPI *s = SIFIVE_SPI(dev);
    int i;

    DB_PRINT("\n");

    s->ssi = ssi_create_bus(dev, "spi");

    sysbus_init_irq(sbd, &s->irq);
    s->cs_lines = g_new0(qemu_irq, s->num_cs);
    for (i = 0; i < s->num_cs; ++i) {
        sysbus_init_irq(sbd, &s->cs_lines[i]);
    }

    memory_region_init_io(&s->mmio, OBJECT(s), &spi_ops, s,
                          "sifive-spi", R_MAX * 4);
    sysbus_init_mmio(sbd, &s->mmio);

    s->irqline = -1;
}

static const VMStateDescription vmstate_sifive_spi = {
    .name = "sifive_spi",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, sifiveSPI, R_MAX),
        VMSTATE_END_OF_LIST()
    }
};

static Property sifive_spi_properties[] = {
    DEFINE_PROP_UINT8("num-ss-bits", sifiveSPI, num_cs, 1),
    DEFINE_PROP_END_OF_LIST(),
};

static void sifive_spi_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = sifive_spi_realize;
    dc->reset = sifive_spi_reset;
    device_class_set_props(dc, sifive_spi_properties);
    dc->vmsd = &vmstate_sifive_spi;
}

static const TypeInfo sifive_spi_info = {
    .name           = TYPE_SIFIVE_SPI,
    .parent         = TYPE_SYS_BUS_DEVICE,
    .instance_size  = sizeof(sifiveSPI),
    .class_init     = sifive_spi_class_init,
};

static void sifive_spi_register_types(void)
{
    type_register_static(&sifive_spi_info);
}

type_init(sifive_spi_register_types)
