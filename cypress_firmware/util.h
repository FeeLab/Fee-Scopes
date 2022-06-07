/*
 * util.h
 *
 *  Created on: Jul 2, 2019
 *      Author: Galen
 */

#ifndef UTIL_H_
#define UTIL_H_

#include <cyu3types.h>

/*
   Function    : FillBuff2B
   Description : Break a unit16_t into its constituent bytes
   Parameters  :
                 input - value to be split
                 buf - buffer to place bytes (must be at least length 2)
   Return  :
                 buf[0] will contain the most significant byte, and buff[1]
                 will contain the least significant.
 */
inline void
FillBuff2B (
    uint16_t input,
    uint8_t *buf)
{
  buf[0] = (uint8_t) ((input & 0xFF00) >> 8);
  buf[1] = (uint8_t) (input & 0x00FF);
}

/*
  Function    : FillBuff2BLsb
  Description : Break a unit16_t into its constituent bytes
  Parameters  :
  input - value to be split
  buf - buffer to place bytes (must be at least length 2)
  Return  :
  buf[0] will contain the most significant byte, and buff[1]
  will contain the least significant.
*/
inline void
FillBuff2BLsb (
            uint16_t input,
            uint8_t *buf)
{
    buf[0] = (uint8_t) (input & 0x00FF);
    buf[1] = (uint8_t) ((input & 0xFF00) >> 8);
}

/*
   Function    : Combine2B
   Description : Combine two bytes into a unit16_t
   Parameters  :
                 buf - buffer containing two bytes (must be at least length 2)
                 MSB should be first, then LSB.

 */
inline uint16_t
Combine2B (
    uint8_t *buf)
{
  return ((uint16_t) buf[0] << 8) | buf[1];
}

#endif /* UTIL_H_ */
