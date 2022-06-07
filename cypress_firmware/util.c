/*
 * util.c
 *
 *  Created on: Jul 2, 2019
 *      Author: Galen
 */
#include "util.h"

extern inline void
FillBuff2B (
    uint16_t input,
    uint8_t *buf);

extern inline void
FillBuff2BLsb (
            uint16_t input,
            uint8_t *buf);

extern inline uint16_t
Combine2B (
    uint8_t *buf);
