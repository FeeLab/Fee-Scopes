#include "usart_out.h"

extern inline void usart_transmit(uint8_t byte_out);
extern inline void usart_transmit_wait(uint8_t byte_out);

void usart_init(void)
{
  /* Setup pins */
  PORTMUX.CTRLB |= PORTMUX_USART0_bm; // Use alternate port

  // Disable input buffering
  PORTA.PIN1CTRL = PORTA.PIN2CTRL = PORTA.PIN3CTRL = PORT_ISC_INPUT_DISABLE_gc;
}

void usart_enable(void)
{
  VPORTA.OUT |= PIN1_bm; // set TxD to high
  VPORTA.OUT &= ~PIN3_bm; // set XCK low
  VPORTA.DIR |= PIN1_bm | PIN3_bm; // set TxD, XCK to output

  /* Set baud to 2.5 MHz
    first ten bits of 16 bit baud register, baud[15:6], hold
    integral part of baud rate. See datasheet (page 255) for calculation
    of register value.
  */
  USART0.BAUDL = 0x40;
  USART0.BAUDH = 0x00;

  /* 8 bit synchronous, 1 stop bit no parity = 10 bit frame */
  USART0.CTRLC = USART_CMODE_SYNCHRONOUS_gc | USART_CHSIZE_8BIT_gc;
  USART0.CTRLB |= USART_TXEN_bm; // Enable transmission
}

void usart_disable(void)
{
  USART0.CTRLB &= ~USART_TXEN_bm; // Disable transmission
}
