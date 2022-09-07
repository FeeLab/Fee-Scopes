#include "spi_master.h"

#include <stdint.h>

#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/atomic.h>

#include "twi_slave.h"
#include "sensor_power_sequencing.h"

/*************
 * CONSTANTS *
 *************/

#define SPI_VPORT VPORTC
#define SPI_PORT  PORTC

#define SCK_bm    PIN0_bm
#define SCK_bp    PIN0_bp
#define SCKCTRL   PIN0CTRL
#define MOSI_bm   PIN2_bm
#define MOSI_bp   PIN2_bp
#define MOSICTRL  PIN2CTRL
#define MISO_bm   PIN1_bm
#define MISO_bp   PIN1_bp
#define MISOCTRL  PIN1CTRL
#define SSN_bm    PIN3_bm
#define SSN_bp    PIN3_bp
#define SSNCTRL   PIN3CTRL

#define SPI_STATE_PERIOD_US 1
#define SPI_CYCLE_PERIOD_US ( 2 * SPI_STATE_PERIOD_US )

/*******************
 * GLOBAL VARIABLES *
 ********************/

static spi_mutex_t spi_lock;

/*************************
 * FUNCTION DECLARATIONS *
 *************************/

static void spi_python480_write_register_addr(uint16_t reg_addr, bool is_write);

/************************
 * FUNCTION DEFINITIONS *
 ************************/

void spi_stop_transaction(void)
{
    SPI_VPORT.OUT |= SSN_bm;
}

void spi_start_transaction(void)
{
    SPI_VPORT.OUT &= ~SSN_bm; // Set SS low
}

void spi_write(uint8_t byte_out, int ntransmit)
{
    int i;
    if (ntransmit > 0 && ntransmit <= 8) {
        byte_out <<= 8 - ntransmit; // Skip over unused bits
        for (i = ntransmit; i > 0; i--) {
            /* Set MOSI to the msb of this byte */
            SPI_VPORT.OUT = (VPORTC.OUT & ~MOSI_bm) |
                ((byte_out >> 7) << MOSI_bp );
            _delay_us(SPI_STATE_PERIOD_US);
            /* assert sck to latch data */
            SPI_VPORT.OUT |= SCK_bm;
            _delay_us(SPI_STATE_PERIOD_US);
            /* de-assert sck for next bit */
            SPI_VPORT.OUT &= ~SCK_bm;
            /* shift the byte */
            byte_out <<= 1;
        }
    }
}

void spi_write_word(uint16_t word_out, int ntransmit)
{
    if (ntransmit <= 16) {
        if (ntransmit > 8) {
            spi_write(word_out >> 8, ntransmit - 8);
            _delay_us(SPI_STATE_PERIOD_US);
            spi_write(word_out, 8);
        } else if (ntransmit > 0) {
            spi_write(word_out, ntransmit);
        }
    }
}

uint8_t spi_read(void) {
    uint8_t byte_in;
    int i;
    // PIN1 is SCK
    // PIN2 is MISO
    /* assert sck so slave sets up next bit */
    SPI_VPORT.OUT |= SCK_bm;
    _delay_us(SPI_STATE_PERIOD_US);
    byte_in = (SPI_VPORT.IN & MISO_bm) >> MISO_bp;
    /* de-assert sck for next bit */
    SPI_VPORT.OUT &= ~SCK_bm;
    for (i = 0; i < 7; i++) {
        /* assert sck so slave sets up next bit */
        _delay_us(SPI_STATE_PERIOD_US);
        SPI_VPORT.OUT |= SCK_bm;
        _delay_us(SPI_STATE_PERIOD_US);
        byte_in <<= 1;
        byte_in |= (SPI_VPORT.IN & MISO_bm) >> MISO_bp;
        /* de-assert sck for next bit */
        SPI_VPORT.OUT &= ~SCK_bm;
    }
    return byte_in;
}

uint16_t spi_read_word(void) {
    uint16_t word_in;
    word_in = spi_read() << 8; // MSB
    _delay_us(SPI_STATE_PERIOD_US);
    word_in |= spi_read(); // LSB
    return word_in;
}

spi_mutex_t current_spi_lock(void) {
    return spi_lock;
}

bool spi_trylock(spi_mutex_t requester)
{
    bool res = false;
    if (spi_lock == SPI_NO_LOCK) {
        switch (requester) {
        case SPI_TWI:
            sequencing_disable_interrupts();
            spi_lock = SPI_TWI;
            break;
        case SPI_MAIN:
            twi_disable_interrupts();
            spi_lock = SPI_MAIN;
            break;
        case SPI_NO_LOCK:
            break;
        }
        res = true;
    } else if (requester == spi_lock) {
        res = true;
    } else {
        res = false;
    }
    return res;
}

bool spi_trylock_atomic(spi_mutex_t requester) {
    bool res = false;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        res = spi_trylock(requester);
    }
    return res;
}

bool spi_tryunlock(spi_mutex_t requester) {
    bool res = false;
    if (requester == spi_lock) {
        switch(spi_lock) {
        case SPI_NO_LOCK:
            break;
        case SPI_TWI:
            sequencing_enable_interrupts();
            break;
        case SPI_MAIN:
            twi_enable_interrupts();
            break;
        }
        spi_lock = SPI_NO_LOCK;
        res = true;
    } else {
        res = false;
    }
    return res;
}

bool spi_tryunlock_atomic(spi_mutex_t requester) {
    bool res;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        res = spi_tryunlock(requester);
    }
    return res;
}

void spi_enable(void)
{
  SPI0.CTRLA |= SPI_ENABLE_bm;
}

void spi_disable(void)
{
  SPI0.CTRLA &= ~SPI_ENABLE_bm;
}

static void spi_python480_write_register_addr(uint16_t reg_addr, bool is_write)
{
    reg_addr <<= 1;
    reg_addr |= is_write;
    spi_write_word(reg_addr, 10);
}

/* Assumes 9 bit register address is at least significant position in reg_addr
 * Assumes spi is enabled and in mode 0 at time of call
 * Leaves spi enabled in mode 0
 */
void spi_python480_write(uint16_t reg_addr, uint16_t reg_value)
{
    spi_start_transaction();
    _delay_us(SPI_CYCLE_PERIOD_US);
    spi_python480_write_register_addr(reg_addr, true);
    _delay_us(SPI_STATE_PERIOD_US);
    spi_write_word(reg_value, 16);
    _delay_us(SPI_CYCLE_PERIOD_US);
    spi_stop_transaction();
    _delay_us(2 * SPI_CYCLE_PERIOD_US);
}

/* Assumes 9 bit register address is at least significant position in reg_addr
 * Assumes spi is enabled and in mode 0 at time of call
 * leaves SPI enabled in mode 0
 */
uint16_t spi_python480_read(uint16_t reg_addr)
{
    uint16_t slave_data;
    spi_start_transaction();
    _delay_us(SPI_CYCLE_PERIOD_US);
    spi_python480_write_register_addr(reg_addr, false);
    _delay_us(SPI_STATE_PERIOD_US);
    slave_data = spi_read_word();
    _delay_us(SPI_CYCLE_PERIOD_US);
    spi_stop_transaction();
    _delay_us(2 * SPI_CYCLE_PERIOD_US);
    return slave_data;
}

/* Descending into the madness of the Python 480 SPI protocol
 *
 * The Python 480 uses nine bit register addresses, transmitted most significant
 * bit first, with one bit of read/write direction, high for a write. The Python
 * 480 requires the clock to be low when idle. The clock should not toggle for
 * one clock period after the SS line is pulled low, and one clock cycle before
 * the SS line is deasserted. The polarity of the latching depends on whether
 * data is being read into or out of the sensor. When writing data to the sensor
 * the sensor latches data on the rising edge of the clock; when reading data
 * from the sensor the ATtiny should latch data on the falling edge of the
 * clock.
 *
 * We are using the Python 480 with the CMOS interface, which means `ratspi` <= 6.
 * The sensor oscillator is a 66.66 MHz, so the maximum SPI frequency is 11.11 MHz, which
 * is faster than the core frequency of the microcontroller. Thus, the maximum SPI
 * frequency that the ATtiny 816 is capable of, 2.5 MHz, is alright. The one cycle
 * minimum between SS and SCK should be satisfied by the delay between instructions
 * being executed by the ATtiny 816.
 *
 * The ATtiny should be in SPI mode 0 during write, and SPI mode 1 during read.
 *
 * Real SCK  is incorrectly wired as MISO (PC1)
 * Real MISO is incorrectly wired as MOSI (PC2)
 * Real MOSI is incorrectly wired as SCK  (PC0)
 * Real SSN  is correctly   wired as SSN  (PC3)
 */
void spi_init(void)
{
  PORTMUX.CTRLB |= PORTMUX_SPI0_bm;

  // Disable input buffering for SCK, MOSI, SS
  SPI_PORT.MOSICTRL = SPI_PORT.SCKCTRL = SPI_PORT.SSNCTRL = PORT_ISC_INPUT_DISABLE_gc;
  SPI_PORT.OUT = SCK_bm; // Set SSN high
  SPI_PORT.DIR = MOSI_bm | SSN_bm | SCK_bm; // SCK, MOSI, SS set to output
}
