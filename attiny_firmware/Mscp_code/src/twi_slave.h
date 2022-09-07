#ifndef TWISLAVE_H
#define TWISLAVE_H

/* TWI slave module
 *
 * Uses interrupts to implement the MCU's TWI interface, and communicate with
 * the main thread with global variables that are maniuplated during the ISR.
 *
 * Hardware used: TWI0, PB0-1, Interrupts
 */

#include <stdint.h>
#include <stdbool.h>

/*******************************
 * INLINE FUNCTION DEFINITIONS *
 *******************************/

inline void twi_enable_interrupts(void)
{
    TWI0.SCTRLA |= TWI_DIEN_bm   |
        TWI_APIEN_bm  |
        TWI_PIEN_bm; // enable all TWI interrupts
}

inline void twi_disable_interrupts(void) {
    TWI0.SCTRLA &= ~TWI_DIEN_bm   &
        ~TWI_APIEN_bm  &
        ~TWI_PIEN_bm; // enable all TWI interrupts
}

/*************************
 * FUNCTION DECLARATIONS *
 *************************/

void twi_enable(void); // start TWI hardware and interrupts
void twi_init(void); // configure TWI hardware to respond to addresses
bool twi_in_transaction(void); // determine if TWI is being used

#endif
