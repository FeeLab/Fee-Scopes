#ifndef USART_OUT_H
#define USART_OUT_H

#include <stdint.h>

#include <avr/io.h>

/* UART module
 *
 * Uses interrupts to implement the MCU's TWI interface, and communicate with
 * the main thread with global variables that are maniuplated during the ISR.
 *
 * Hardware used: TWI0, PB0-1, Interrupts
 */

/*******************************
 * INLINE FUNCTION DEFINITIONS *
 *******************************/

// The USART does not use inverted output (e.g. low is 0), and transmits the least significant bit first.
inline void usart_transmit(uint8_t byte_out)
{
  USART0.TXDATAL = byte_out;
}

inline void usart_transmit_wait(uint8_t byte_out)
{
  while (!(USART0.STATUS & USART_DREIF_bm)) {}; // Wait for write buffer to be empty
  usart_transmit(byte_out);
}

/*************************
 * FUNCTION DECLARATIONS *
 *************************/

void usart_init(void);
void usart_enable(void);
void usart_disable(void);

#endif
