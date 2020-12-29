#ifndef QEMU_CRC16_H
#define QEMU_CRC16_H

uint16_t crc16_ccitt(uint16_t crc, const uint8_t *buffer, unsigned int len);

#endif
