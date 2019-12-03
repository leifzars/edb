/*
 * crc.h
 *
 */

#ifndef LIB_CRC_H_
#define LIB_CRC_H_

#include <inttypes.h>
#include <stdio.h>

uint32_t rc_crc32(uint32_t crc, const char *buf, size_t len);

#endif /* LIB_CRC_H_ */
