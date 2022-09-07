#ifndef SPI_MASTER_H
#define SPI_MASTER_H

/* SPI Master
 *
 * Bit-banged implementation of SPI master. The SPI0 hardware module is not used
 * because the SPI lines were incorrectly wired to the ATtiny in rev 2.0 of the
 * microscope.
 *
 * Hardware used: PC0-3
 */

#include <stdint.h>
#include <stdbool.h>

#include <avr/io.h>

/************
 * TYPEDEFS *
 ************/

typedef enum {
              SPI_NO_LOCK,
              SPI_TWI,
              SPI_MAIN
} spi_mutex_t;

/*************************
 * FUNCTION DECLARATIONS *
 *************************/

/* Initialize hardware */
void spi_init(void);

/* Mutex functions
 *
 * These functions allow either the main thread or the TWI interrupts to execute
 * SPI transactions without interruptions. When the mutex is held by one of the
 * systems, it disables the interrupts for the other system. Callers to these
 * functions must specify if the requesting code is either on the main thread,
 * with `SPI_MAIN`, or instead TWI interrupts, with `SPI_TWI`. They will return
 * `true` if the lock/unlock operation was successful. `atomic` variants disable
 * interrupts before attempting to get the lock, which should be used for all
 * callers outside of a ISR, when further interrupts would be impossible
 * anyways.
 */

bool spi_trylock(spi_mutex_t requester);
bool spi_trylock_atomic(spi_mutex_t requester);
bool spi_tryunlock(spi_mutex_t requester);
bool spi_tryunlock_atomic(spi_mutex_t requester);

/* SPI functions */

/* Assert SSN */
void spi_start_transaction(void);

/* De-assert SSN */
void spi_stop_transaction(void);

/* Read a byte using SPI phase 1, assumes SSN already asserted */
uint8_t spi_read(void);

/* Read two bytes using SPI phase 1, assumes SSN already asserted */
uint16_t spi_read_word(void);

/* void spi_write(uint8_t byte_out, int ntransmit)
 *
 * Writes `ntransmit` bits of `byte_out` using SPI phase 0, assumes SSN already
 * asserted.
 */
void spi_write(uint8_t byte_out, int ntransmit);

/* void spi_write_word(uint16_t word_out, int ntransmit)
 *
 * Writes `ntransmit` bits of `word_out` using SPI phase 0, assumes SSN already
 * asserted.
 */
void spi_write_word(uint16_t word_out, int ntransmit);

/* uint16_t spi_python480_read(uint16_t reg_addr)
 *
 * Reads the register word at address `reg_addr`, according to the Python480
 * standard.
 */
uint16_t spi_python480_read(uint16_t reg_addr);

/* void spi_python480_write(uint16_t reg_addr, uint16_t reg_value)
 *
 * Writes the register word `reg_value` to register at address `reg_addr`,
 * according to the Python480 standard.
 */
void spi_python480_write(uint16_t reg_addr, uint16_t reg_value);

#endif
