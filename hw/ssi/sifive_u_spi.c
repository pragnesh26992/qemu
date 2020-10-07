/*
 * Arm PrimeCell PL022 Synchronous Serial Port
 *
 * Copyright (c) 2020 SiFIve,Inc
 * Written by Pragnesh Patel
 *
 * This code is licensed under the GPL.
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "migration/vmstate.h"
#include "hw/irq.h"
#include "hw/ssi/ssi.h"
#include "qemu/log.h"
#include "qemu/module.h"

#include "hw/riscv/sifive_u_spi.h"
#include "hw/qdev-properties.h"

//#define SIFIVE_SPI_ERR_DEBUG 1

#ifdef SIFIVE_SPI_ERR_DEBUG
#define DB_PRINT(...) do { \
    fprintf(stderr,  ": %s: ", __func__); \
    fprintf(stderr, ## __VA_ARGS__); \
    } while (0)
#else
    #define DB_PRINT(...)
#endif


static void sifive_spi_transfer(sifiveSPI *s)
{
    
    s->regs[R_RXDATA] = ssi_transfer(s->ssi, s->regs[R_TXDATA]);

}

static uint64_t
spi_read(void *opaque, hwaddr addr, unsigned int size)
{
    sifiveSPI *s = opaque;
    uint32_t r = 0;

    addr >>= 2;
    switch (addr) {
    case R_RXDATA:
//	s->regs[R_IP] |= SIFIVE_SPI_IP_RXWM;

        r = s->regs[R_RXDATA];
    	DB_PRINT("Data received: 0x%x\n", s->regs[R_RXDATA]);
        break;
    case R_IP:
	s->regs[R_IP] |= SIFIVE_SPI_IP_RXWM;
	r = s->regs[R_IP];
	break;
    default:
        if (addr < ARRAY_SIZE(s->regs)) {
            r = s->regs[addr];
        }
        break;
    }

//    DB_PRINT("addr=" TARGET_FMT_plx " = %x\n", addr * 4, r);
    return r;
}

static void
spi_write(void *opaque, hwaddr addr,
            uint64_t val64, unsigned int size)
{
    sifiveSPI *s = opaque;
    uint32_t value = val64;

    addr >>= 2;
    switch (addr) {
    case R_TXDATA:
	s->regs[R_TXDATA] = (unsigned)value;
    	DB_PRINT("Data to send: 0x%x\n", s->regs[R_TXDATA]);
        sifive_spi_transfer(s);
	s->regs[R_IP] |= SIFIVE_SPI_IP_TXWM;
	break;
    default:
        if (addr < ARRAY_SIZE(s->regs)) {
            s->regs[addr] = value;
        }
        break;
    }
}

static void sifive_spi_do_reset(sifiveSPI *s)
{
    memset(s->regs, 0, sizeof s->regs);

    s->rx_fifo_len = 0;
    s->rx_fifo_head = 0;
    s->tx_fifo_len = 0;
    s->tx_fifo_head = 0;
    s->regs[R_TXMARK] = 1;
    s->regs[R_CSDEF] = 1;
    s->regs[R_IP] = (SIFIVE_SPI_IP_TXWM | SIFIVE_SPI_IP_RXWM);
}

static void sifive_spi_reset(DeviceState *d)
{
    sifive_spi_do_reset(SIFIVE_SPI(d));
}

static const MemoryRegionOps spi_ops = {
    .read = spi_read,
    .write = spi_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

#if 0
static int pl022_post_load(void *opaque, int version_id)
{
    PL022State *s = opaque;

    if (s->tx_fifo_head < 0 ||
        s->tx_fifo_head >= ARRAY_SIZE(s->tx_fifo) ||
        s->rx_fifo_head < 0 ||
        s->rx_fifo_head >= ARRAY_SIZE(s->rx_fifo)) {
        return -1;
    }
    return 0;
}
#endif

static const VMStateDescription vmstate_sifive_spi = {
    .name = "sifive_spi",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, sifiveSPI, R_MAX),
        VMSTATE_END_OF_LIST()
    }
};

static void sifive_spi_realize(DeviceState *dev, Error **errp)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    sifiveSPI *s = SIFIVE_SPI(dev);
    //int i;

    DB_PRINT("\n");

    s->ssi = ssi_create_bus(dev, "ssi");

    sysbus_init_irq(sbd, &s->irq);

#if 0
    s->cs_lines = g_new0(qemu_irq, s->num_cs);
    for (i = 0; i < s->num_cs; ++i) {
        sysbus_init_irq(sbd, &s->cs_lines[i]);
    }
#endif

    memory_region_init_io(&s->mmio, OBJECT(s), &spi_ops, s,
                          "sifive-spi", R_MAX * 4);
    sysbus_init_mmio(sbd, &s->mmio);
}

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
