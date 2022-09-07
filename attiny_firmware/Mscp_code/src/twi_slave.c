#include <stdbool.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#include "twi_slave.h"
#include "spi_master.h"
#include "adc.h"
#include "sensor_power_sequencing.h"

/************************
 * TWI SLAVE ADDRESSES  *
 ************************/

#define TWI_SPI_ADDR_WR 0xB8 // SENSOR_ADDR used in sensor.c of FX3 firmware, 8 bit
#define TWI_SPI_ADDR_RD ( TWI_SPI_ADDR_WR | 0x01 )
#define TWI_SELF_ADDR_WR 0x98 // DAC_ADDR used in sensor.c of FX3 firmware, 8 bit
#define TWI_SELF_ADDR_RD ( TWI_SELF_ADDR_WR | 0x01 )

/*************************
 * TWI PSEUDO-REGISTERS  *
 *************************/

/* TWI_SELF_REG_SENSOR_CTRL
 *
 * Register to control the image sensor over TWI
 * bits are [pq00 00cd]
 * p = power, 1 for on
 * q = power acknowledge, read only, set if image sensor is transitioning power
 * states
 * c = configure image sensor over spi
 * d = configure acknowledge, read only, set if image sensor is waiting to be configured
 */
#define TWI_SELF_REG_SENSOR_CTRL 0x00

#define SELF_SENSOR_CONFIG_ACK_bp 0
#define SELF_SENSOR_CONFIG_ACK_bm ( 1 << SELF_SENSOR_CONFIG_ACK_bp )
#define SELF_SENSOR_CONFIGURE_bp  1
#define SELF_SENSOR_CONFIGURE_bm  ( 1 << SELF_SENSOR_CONFIGURE_bp )
#define SELF_SENSOR_POWER_ACK_bp  6
#define SELF_SENSOR_POWER_ACK_bm  ( 1 << SELF_SENSOR_POWER_ACK_bp )
#define SELF_SENSOR_POWER_bp      7
#define SELF_SENSOR_POWER_bm      ( 1 << SELF_SENSOR_POWER_bp )

/* TWI_SELF_REG_ADC_CTRL
 *
 * Register to control the ADC over TWI
 * bits are [0000 00me]
 * e = enable, 1 for on
 * m = multiplex, 1 for true
 */
#define TWI_SELF_REG_ADC_CTRL 0x01

#define SELF_ADC_CTRL_ENABLE_bp    0
#define SELF_ADC_CTRL_ENABLE_bm    ( 1 << SELF_ADC_CTRL_ENABLE_bp )
#define SELF_ADC_CTRL_MULTIPLEX_bp 1
#define SELF_ADC_CTRL_MULTIPLEX_bm ( 1 << SELF_ADC_CTRL_MULTIPLEX_bp )

/* TWI_SELF_REG_ADC_PER
 *
 * Register to control the ADC period in clock cycles
 * 16 bit word
 */
#define TWI_SELF_REG_ADC_PER 0x02

/*************
 * CONSTANTS *
 *************/

#define TWI_BUS_ERROR_MASK   0x0C

// These assume the TWC and TWBE flags have been masked out
#define TWI_ADDR_R_ACK       0x63 // slave has been addressed with read, master last acked
#define TWI_ADDR_R_NACK      0x73 // slave has been addressed with read, master last nacked
#define TWI_ADDR_W_ACK       0x61 // slave has been addressed with write, master last acked
#define TWI_ADDR_W_NACK      0x71  // slave has been addressed with write, master last nacked

#define TWI_STOP_R_ACK       0x42
#define TWI_STOP_R_NACK      0x52
#define TWI_STOP_W_ACK       0x40
#define TWI_STOP_W_NACK      0x50

#define TWI_STOP_HOLD_R_ACK  0x62
#define TWI_STOP_HOLD_R_NACK 0x72
#define TWI_STOP_HOLD_W_ACK  0x60
#define TWI_STOP_HOLD_W_NACK 0x70

#define TWI_DATA_R_ACK       0xA3
#define TWI_DATA_R_NACK      0xB3
#define TWI_DATA_W_ACK       0xA1
#define TWI_DATA_W_NACK      0xB1

/************
 * TYPEDEFS *
 ************/

/* States for state machine
 *
 * This enum tracks the current state of the TWI transaction, and is intended
 * to be used with revision 2.0 of the FeatherScope. At the beginning of a TWI
 * transaction, the TWI interrupt tries to get the SPI lock, and will hold it
 * until the TWI transaction state returns to TWI_IDLE.
 */
typedef enum {
              /* TWI_IDLE
               *
               * Outside of a TWI transaction
               *
               * State
               * =====
               * persist_b: undetermined
               * SSN: not asserted
               * SPI transaction: no transaction
               * SPI lock: not held
               *
               * Transitions
               * ===========
               * TWI_ADDR_W_* -> TWI_SPI_TRANSACT, TWI_SELF_TRANSACT
               * default      -> TWI_IDLE
               *
               * Actions
               * =======
               * TWI_ADDR_W_* -> Ack, if match, nack else
               */
              TWI_IDLE,

              /* TWI_SPI_TRANSACT
               *
               * Starting SPI transaction
               *
               * State
               * =====
               * persist_b: undetermined
               * SSN: not asserted
               * SPI transaction: no transaction
               * SPI lock: held
               *
               * Transitions
               * ===========
               * TWI_DATA_W_* -> TWI_SPI_REG1
               * TWI_ADDR_W_* -> TWI_SPI_TRANSACT, TWI_SELF_TRANSACT
               * default      -> TWI_IDLE
               *
               * Actions
               * =======
               * TWI_DATA_W_* -> Ack
               */
              TWI_SPI_TRANSACT,

              /* TWI_SPI_REG1
               *
               * In SPI transaction, know the first bit of the register
               *
               * State
               * =====
               * persist_b: contains first bit of register address
               * SSN: not asserted
               * SPI transaction: no transaction
               * SPI lock: held
               *
               * Transitions
               * ===========
               * TWI_DATA_W_* -> TWI_SPI_REG2
               * TWI_ADDR_W_* -> TWI_SPI_TRANSACT, TWI_SELF_TRANSACT
               * default      -> TWI_IDLE
               *
               * Actions
               * =======
               * TWI_DATA_W_* -> Ack, assert SSN, write MSB of register address
               *                 over SPI
               * TWI_ADDR_W_* -> Ack
               */
              TWI_SPI_REG1,

              /* TWI_SPI_REG2
               *
               * In SPI transaction, know the register
               *
               * State
               * =====
               * persist_b: contains last bit of register address
               * SSN: asserted
               * SPI transaction: first 8 bits of register address transmitted
               * SPI lock: held
               *
               * Transitions
               * ===========
               * TWI_DATA_W_* -> TWI_SPI_FIRST_WRITE
               * TWI_ADDR_R_* -> TWI_SPI_FIRST_READ
               * TWI_ADDR_W_* -> TWI_SPI_TRANSACT, TWI_SELF_TRANSACT
               * default      -> TWI_IDLE
               *
               * Actions
               * =======
               * TWI_DATA_W_* -> Ack, write last two bits of register address,
               *                 and then write the MSB of the register word
               *                 over SPI
               * TWI_ADDR_R_* -> Ack, write last two bits of register address,
               *                 and then read the MSB of the register word over
               *                 SPI
               * TWI_ADDR_W_* -> Ack
               */
              TWI_SPI_REG2,

              /* TWI_SPI_WRITE
               *
               * Write over SPI, LSB of word
               *
               * State
               * =====
               * persist_b: undetermined
               * SSN: asserted
               * SPI transaction: register address transmitted with write bit
               *                  set, one byte of data already transmitted.
               * SPI lock: held
               *
               * Transitions
               * ===========
               * TWI_DATA_W_* -> TWI_AWAIT_STOP
               * TWI_ADDR_W_* -> TWI_SPI_TRANSACT, TWI_SELF_TRANSACT
               * default      -> TWI_IDLE
               *
               * Actions
               * =======
               * all          -> de-assert SSN
               * TWI_DATA_W_* -> Ack, transmit LSB of register word over SPI
               * TWI_ADDR_W_* -> Ack
               */
              TWI_SPI_WRITE,

              /* TWI_SPI_FIRST_READ
               *
               * Read over SPI, MSB of word
               *
               * State
               * =====
               * persist_b: MSB of register word
               * SSN: asserted
               * SPI transaction: register transmitted with write bit cleared,
               *                  one byte of data already read.
               * SPI lock: held
               *
               * Transitions
               * ===========
               * TWI_DATA_R_* -> TWI_SPI_SECOND_READ
               * TWI_ADDR_W_* -> TWI_SPI_TRANSACT, TWI_SELF_TRANSACT
               * default      -> TWI_IDLE
               *
               * Actions
               * =======
               * all          -> de-assert SSN
               * TWI_DATA_R_* -> Transmit MSB of word over TWI, read LSB of
                                 register word over SPI
               * TWI_ADDR_W_* -> Ack
               */
              TWI_SPI_FIRST_READ,

              /* TWI_SPI_SECOND_READ
               *
               * Read over SPI, LSB of word
               *
               * State
               * =====
               * persist_b: LSB of register word
               * SSN: not asserted
               * SPI transaction: two bytes of data already read, transaction
               *                  finished.
               * SPI lock: held
               *
               * Transitions
               * ===========
               * TWI_DATA_R_* -> TWI_FINISH_READ
               * TWI_ADDR_W_* -> TWI_SPI_TRANSACT, TWI_SELF_TRANSACT
               * default      -> TWI_IDLE
               *
               * Actions
               * =======
               * TWI_DATA_R_ACK -> Transmit LSB of register word over TWI
               * TWI_ADDR_W_*   -> Ack
               */
              TWI_SPI_SECOND_READ,

              /* TWI_SELF_TRANSACT
               *
               * Starting self transaction
               *
               * State
               * =====
               * persist_b: undetermined
               * SSN: not asserted
               * SPI transaction: no transaction
               * SPI lock: held
               *
               * Transitions
               * ===========
               * TWI_DATA_W_* -> TWI_SELF_REG
               * TWI_ADDR_W_* -> TWI_SPI_TRANSACT, TWI_SELF_TRANSACT
               * default      -> TWI_IDLE
               *
               * Actions
               * =======
               * TWI_DATA_W_* -> Ack
               */
              TWI_SELF_TRANSACT,


              /* TWI_SELF_REG
               *
               * Self transaction: register known
               *
               * State
               * =====
               * persist_b: holds register address
               * SSN: not asserted
               * SPI transaction: no transaction
               * SPI lock: held
               *
               * Transitions
               * ===========
               * TWI_DATA_W_* -> TWI_AWAIT_STOP, TWI_SELF_WRITE
               * TWI_ADDR_W_* -> TWI_SPI_TRANSACT, TWI_SELF_TRANSACT
               * default      -> TWI_IDLE
               *
               * Actions
               * =======
               * TWI_DATA_W_* -> Ack, if register address is
               *                 TWI_SELF_REG_ADC_CTRL, then enact ADC action.
               *                 If instead register address is
               *                 TWI_SELF_REG_SENSOR_CTRL, then enact sensor
               *                 control.
               * TWI_DATA_W_* -> Ack
               */
              TWI_SELF_REG, // SSN is not asserted, TWSME disabled, persist_b holds register value

              /* TWI_SELF_WRITE
               *
               * Second write to own register, which is only possible with the
               * ADC period register.
               *
               * State
               * =====
               * persist_b: holds MSB of register word
               * SSN: not asserted
               * SPI transaction: no transaction
               * SPI lock: held
               *
               * Transitions
               * ===========
               * TWI_DATA_W_* -> TWI_AWAIT_STOP
               * TWI_ADDR_W_* -> TWI_SPI_TRANSACT, TWI_SELF_TRANSACT
               * default      -> TWI_IDLE
               *
               * Actions
               * =======
               * TWI_DATA_W_* -> Ack, set ADC sample period
               */
              TWI_SELF_WRITE,

              /* TWI_SELF_FIRST_READ
               *
               * Read MSB of own register value
               *
               * State
               * =====
               * persist_b: holds register
               * SSN: not asserted
               * SPI transaction: no transaction
               * SPI lock: held
               *
               * Transitions
               * ===========
               * TWI_DATA_R_* -> TWI_FINISH_READ, TWI_SELF_SECOND_READ
               * TWI_ADDR_W_* -> TWI_SPI_TRANSACT, TWI_SELF_TRANSACT
               * default      -> TWI_IDLE
               *
               * Actions
               * =======
               * TWI_DATA_R_* -> Transmit MSB of register word over TWI.
               */
              TWI_SELF_FIRST_READ,

              /* TWI_SELF_SECOND_READ
               *
               * Read LSB of own register value
               *
               * State
               * =====
               * persist_b: undetermined
               * SSN: not asserted
               * SPI transaction: no transaction
               * SPI lock: held
               *
               * Transitions
               * ===========
               * TWI_DATA_R_* -> TWI_FINISH_READ
               * TWI_ADDR_W_* -> TWI_SPI_TRANSACT, TWI_SELF_TRANSACT
               * default      -> TWI_IDLE
               *
               * Actions
               * =======
               * TWI_DATA_R_ACK -> Transmit LSB of register word over TWI
               */
              TWI_SELF_SECOND_READ,

              /* TWI_FINISH_READ
               *
               * Wait for master to nack last byte
               *
               * State
               * =====
               * persist_b: undetermined
               * SSN: not asserted
               * SPI transaction: no transaction
               * SPI lock: held
               *
               * Transitions
               * ===========
               * TWI_DATA_R_NACK -> TWI_AWAIT_STOP
               * TWI_ADDR_W_*    -> TWI_SPI_TRANSACT, TWI_SELF_TRANSACT
               * default         -> TWI_IDLE
               *
               * Actions
               * =======
               */
              TWI_FINISH_READ,

              /* TWI_AWAIT_STOP
               *
               * Wait for stop
               *
               * State
               * =====
               * persist_b: undetermined
               * SSN: not asserted
               * SPI transaction: no transaction
               * SPI lock: held
               *
               * Transitions
               * ===========
               * TWI_DATA_R_* -> TWI_IDLE
               * TWI_ADDR_W_* -> TWI_SPI_TRANSACT, TWI_SELF_TRANSACT
               * default      -> TWI_IDLE
               *
               * Actions
               * =======
               */
              TWI_AWAIT_STOP,
} twi_state_t;

/*******************
* GLOBAL VARIABLES *
********************/

/* This global variable stores the current state of the TWI transaction */
static volatile twi_state_t twi_transaction_state = TWI_IDLE;

/*************************
 * FUNCTION DECLARATIONS *
 *************************/

static inline void twi_continue_transaction(void);

/* Finish transaction with nack */
static inline void twi_abort_transaction(void);

/* Finish transaction with ack */
static inline void twi_finish_transaction(void);

/* Continue TWI transaction, then go to TWI_FINISH_READ */
static void twi_transmit_last_byte(void);

/* void twi_abort_idle(void)
 *
 * Terminate TWI transaction with NACK, then set `twi_transaction_state` to
 * TWI_IDLE.
 */
static void twi_abort_idle(void);

/* void twi_wrapup_transaction(void)
 *
 * Terminate TWI transaction, then set `twi_transaction_state` to TWI_IDLE.
 */
static inline void twi_wrapup_transaction(void);

/* void twi_wait_stop(void)
 *
 * Terminate TWI transaction, then set `twi_transaction_state` to TWI_AWAIT_STOP.
 */
static inline void twi_wait_stop(void);

/* void twi_start_transaction(void)
 *
 * Try to get SPI lock, and check for address match. If the address matches
 * either the SPI bridge address, `TWI_SPI_ADDR`, or the self address,
 * `TWI_SELF_ADDR`, then continue the TWI transaction and set the
 * `twi_transaction_state` to either `TWI_SPI_TRANSACT` or `TWI_SELF_TRANSACT` if the
 * respective TWI address was received. If address does not match, or the SPI
 * mutex cannot be locked (also should not happen), then abort the TWI
 * transaction with NACK and go to `TWI_IDLE`.
 */
static void twi_start_transaction(void);

/* void twi_handle_error(void)
 *
 * Respond to bus or transaction errors. If a new write address has been
 * received, then continue the TWI transaction, retain the SPI lock, and call
 * `twi_start_transaction` to determine how to respond to the address.
 * Otherwise, abort the TWI transaction with a NACK and set `twi_transaction_state` to
 * TWI_IDLE.
 */
static void twi_handle_error(uint8_t slave_state);

/* void adc_ctrl_reg_handle(uint8_t reg_value)
 *
 * Parse register word and implement the interface described in
 * `TWI_SELF_REG_ADC_CTRL`.
 */
static inline void adc_ctrl_reg_handle(uint8_t reg_value);

/* uint8_t adc_ctrl_read_reg(void)
 *
 * Generate register word corresponding to description in
 * `TWI_SELF_REG_ADC_CTRL`.
 */
static inline uint8_t adc_ctrl_read_reg(void);

/* void sensor_ctrl_reg_handle(uint8_t reg_value)
 *
 * Parse register word and implement the interface described in
 * `TWI_SELF_REG_SENSOR_CTRL`.
 */
static inline void sensor_ctrl_handle_reg(uint8_t reg_value);

/* uint8_t adc_ctrl_read_reg(void)
 *
 * Generate register word corresponding to description in
 * `TWI_SELF_REG_ADC_CTRL`.
 */
static inline uint8_t sensor_ctrl_read_reg(void);

/*****************************************
 * EXTERN INLINE DECLARATIONS FOR LINKER *
 *****************************************/

extern inline void twi_enable_interrupts(void);
extern inline void twi_disable_interrupts(void);

/************************
 * FUNCTION DEFINITIONS *
 ************************/

void twi_init(void)
{
    TWI0.SADDR = TWI_SPI_ADDR_WR;  // Set own TWI slave address and don't listen to general calls
    TWI0.SADDRMASK = TWI_SELF_ADDR_WR | TWI_ADDREN_bm; // second slave address for ADC
}

void twi_enable(void)
{
    TWI0.SCTRLA = TWI_ENABLE_bm; // enable TWI and all TWI interrupts
}

bool twi_in_transaction(void)
{
    return twi_transaction_state != TWI_IDLE;
}

static inline void twi_continue_transaction(void)
{
    TWI0.SCTRLB = TWI_SCMD_RESPONSE_gc; // ack, wait for byte
}

static inline void twi_abort_transaction(void)
{
    TWI0.SCTRLB = TWI_ACKACT_NACK_gc | TWI_SCMD_COMPTRANS_gc; // nack, end transaction
}

static inline void twi_finish_transaction(void)
{
    TWI0.SCTRLB = TWI_SCMD_COMPTRANS_gc; // ack, end transaction
}

static inline void twi_wrapup_transaction(void)
{
    spi_tryunlock(SPI_TWI);
    twi_transaction_state = TWI_IDLE;
}

static inline void twi_wait_stop(void)
{
    twi_finish_transaction();
    twi_transaction_state = TWI_AWAIT_STOP;
}

static void twi_transmit_last_byte(void)
{
    twi_continue_transaction();
    twi_transaction_state = TWI_FINISH_READ;
}

static void twi_abort_idle(void)
{
    twi_abort_transaction();
    twi_wrapup_transaction();
}

static void twi_start_transaction(void)
{
    uint8_t addr; // Address used to address the attiny20

    addr = TWI0.SDATA;
    if (spi_trylock(SPI_TWI)) {
        if (addr == TWI_SPI_ADDR_WR) {
                twi_continue_transaction();
                twi_transaction_state = TWI_SPI_TRANSACT;
        } else if (addr == TWI_SELF_ADDR_WR) {
                twi_continue_transaction();
                twi_transaction_state = TWI_SELF_TRANSACT;
        } else {
            /* The hardware should prevent this case */
            twi_abort_idle();
        }
    } else {
        twi_abort_idle();
    }
}

static void twi_handle_error(uint8_t slave_state)
{
    TWI0.SSTATUS |= TWI_BUS_ERROR_MASK; // Clear error bits
    slave_state &= ~TWI_BUS_ERROR_MASK;
    switch (slave_state) {
        /* Unexpected restart */
    case TWI_ADDR_W_ACK:
    case TWI_ADDR_W_NACK:
        twi_start_transaction();
        break;
    default:
        twi_abort_idle();
    }
}

static inline void adc_ctrl_reg_handle(uint8_t reg_value)
{
    bool new_adc_en = reg_value & SELF_ADC_CTRL_MULTIPLEX_bm;
    bool new_adc_mult = reg_value & SELF_ADC_CTRL_ENABLE_bm;

    if (new_adc_en && ADC_SAMPLE_REQ && (ADC_MULTIPLEX_REQ != new_adc_mult)) {
        // Need to re-enter the ADC sample loop
        ADC_SAMPLE_REQ = false;
        ADC_UPDATE_REQ = true;
    }
    else {
        ADC_SAMPLE_REQ = new_adc_en;
    }
    ADC_MULTIPLEX_REQ = new_adc_mult;
}

static inline uint8_t adc_ctrl_read_reg(void)
{
    uint8_t reg_val = 0x00 |
        (ADC_MULTIPLEX_REQ << SELF_ADC_CTRL_MULTIPLEX_bp) |
        (ADC_SAMPLE_REQ << SELF_ADC_CTRL_ENABLE_bp);
    return reg_val;
}

static inline void sensor_ctrl_handle_reg(uint8_t reg_value)
{
    if (reg_value & SELF_SENSOR_POWER_bm) {
        SENSOR_POWER_REQ = REQ_POWER_UP;
        if (reg_value & SELF_SENSOR_CONFIGURE_bm) {
            SENSOR_CONFIG_REQ = true;
        }
    } else {
        SENSOR_POWER_REQ = REQ_POWER_DOWN;
    }
}

static inline uint8_t sensor_ctrl_read_reg(void)
{
    bool sensor_power_ack;
    uint8_t sensor_status = 0;

    sensor_power_ack = (SENSOR_POWER_REQ != NO_POWER_REQ) |
        sensor_is_changing_power();

    /* power acknowledge */
    sensor_status |= sensor_power_ack << SELF_SENSOR_POWER_ACK_bp;

    /* configure acknowledge */
    sensor_status |= SENSOR_CONFIG_REQ << SELF_SENSOR_CONFIG_ACK_bp;

    sensor_status |= sensor_is_configured() << SELF_SENSOR_CONFIGURE_bp;

    sensor_status |= sensor_is_on() << SELF_SENSOR_POWER_bp;

    return sensor_status;
}

/********
 * ISRS *
 ********/

ISR(TWI0_TWIS_vect)
{
    static uint8_t persist_b = 0; // used to store bytes between interrupts
    uint8_t temp_b, slave_state;
    uint16_t temp_w;

    /* Get state at start of ISR to avoid race conditions (maybe?) */
    slave_state = TWI0.SSTATUS;

    switch (twi_transaction_state) {

    case TWI_IDLE:
        switch (slave_state) {
        case TWI_ADDR_W_ACK:
        case TWI_ADDR_W_NACK:
            /* this is an address */
            twi_start_transaction();
            break;
        default:
            twi_abort_idle();
            break;
        }
        break;

    case TWI_SPI_TRANSACT:
        switch(slave_state) {
        case TWI_DATA_W_ACK:
        case TWI_DATA_W_NACK:
            /* Master wants to interact with SPI register */
            persist_b = TWI0.SDATA; // Store MSB of register address
            twi_continue_transaction();
            twi_transaction_state = TWI_SPI_REG1;
            break;
        default:
            twi_handle_error(slave_state);
            break;
        }
        break;

    case TWI_SPI_REG1:
        switch(slave_state) {
        case TWI_DATA_W_ACK:
        case TWI_DATA_W_NACK:
            /* Master wants to interact with SPI register */
            temp_b = TWI0.SDATA; // LSB of register address
            twi_continue_transaction();
            spi_start_transaction();
            /* transmit first byte of 9-bit register address over SPI */
            spi_write(persist_b << 7 | temp_b >> 1, 8);
            persist_b = temp_b & 0x01; // last bit of address
            twi_transaction_state = TWI_SPI_REG2;
            break;
        default:
            twi_handle_error(slave_state);
            break;
        }
        break;

    case TWI_SPI_REG2:
        switch(slave_state) {
        case TWI_DATA_W_ACK:
        case TWI_DATA_W_NACK:
            /* Master want to write data over SPI */
            temp_b = TWI0.SDATA;
            twi_continue_transaction();
            /* write last bit of address with direction bit set */
            spi_write((persist_b << 1) | 0x01, 2);
            spi_write(temp_b, 8); // Write MSB of register word over SPI
            twi_transaction_state = TWI_SPI_WRITE;
            break;
        case TWI_ADDR_R_ACK:
        case TWI_ADDR_R_NACK:
            /* Master want to read data over SPI */
            twi_continue_transaction();
            /* write last bit of address with direction bit cleared */
            spi_write(persist_b << 1, 2);
            persist_b = spi_read(); // Read MSB of register word over SPI
            twi_transaction_state = TWI_SPI_FIRST_READ;
            break;
        default:
            spi_stop_transaction();
            twi_handle_error(slave_state);
            break;
        }
        break;

    case TWI_SPI_FIRST_READ:
        switch (slave_state) {
            /* Master want to read data over SPI */
        case TWI_DATA_R_ACK:
        case TWI_DATA_R_NACK:
            TWI0.SDATA = persist_b; // Transmit MSB of register word over TWI
            twi_continue_transaction();
            persist_b = spi_read(); // Read LSB of register word over SPI
            spi_stop_transaction();
            twi_transaction_state = TWI_SPI_SECOND_READ;
            break;
        default:
            spi_stop_transaction();
            twi_handle_error(slave_state);
            break;
        }
        break;

    case TWI_SPI_SECOND_READ:
        switch (slave_state) {
        case TWI_DATA_R_ACK:
            /* Master wants to read data over SPI */
            TWI0.SDATA = persist_b; // Transmit LSB of register word over TWI
            twi_transmit_last_byte();
            break;
        default:
            twi_handle_error(slave_state);
            break;
        }
        break;

    case TWI_SPI_WRITE:
        switch(slave_state) {
        case TWI_DATA_W_ACK:
        case TWI_DATA_W_NACK:
            /* Master wants to transmit over SPI */
            temp_b = TWI0.SDATA; // LSB of register word
            twi_wait_stop();
            spi_write(temp_b, 8);
            spi_stop_transaction();
            break;
        default:
            spi_stop_transaction();
            twi_handle_error(slave_state);
            break;
        }
        break;

    case TWI_SELF_TRANSACT:
        switch(slave_state) {
        case TWI_DATA_W_ACK:
        case TWI_DATA_W_NACK:
            persist_b = TWI0.SDATA; // Store register address
            twi_continue_transaction();
            twi_transaction_state = TWI_SELF_REG;
            break;
        default:
            twi_handle_error(slave_state);
            break;
        }
        break;

    case TWI_SELF_REG:
        switch(slave_state) {
        case TWI_DATA_W_ACK:
        case TWI_DATA_W_NACK:
            /* Master wants to write to this register */
            switch (persist_b) { // persist_b holds register address
            case TWI_SELF_REG_ADC_CTRL:
                temp_b = TWI0.SDATA;
                twi_wait_stop();
                adc_ctrl_reg_handle(temp_b);
                break;
            case TWI_SELF_REG_ADC_PER:
                persist_b = TWI0.SDATA; // MSB of ADC period
                twi_continue_transaction();
                twi_transaction_state = TWI_SELF_WRITE;
                break;
            case TWI_SELF_REG_SENSOR_CTRL:
                temp_b = TWI0.SDATA;
                twi_wait_stop();
                sensor_ctrl_handle_reg(temp_b);
                break;
            default:
                twi_abort_idle();
                break;
            }
            break;
        case TWI_ADDR_R_ACK:
        case TWI_ADDR_R_NACK:
            /* Master wants to read from this register */
            twi_continue_transaction();
            twi_transaction_state = TWI_SELF_FIRST_READ;
            break;
        default:
            twi_handle_error(slave_state);
        }
        break;

    case TWI_SELF_FIRST_READ:
        switch(slave_state) {
        case TWI_DATA_R_ACK:
        case TWI_DATA_R_NACK:
            /* MSB of register word */
            switch (persist_b) { // persist_b holds register address
            case TWI_SELF_REG_SENSOR_CTRL:
                TWI0.SDATA = sensor_ctrl_read_reg();
                twi_transmit_last_byte();
                break;
            case TWI_SELF_REG_ADC_CTRL:
                TWI0.SDATA = adc_ctrl_read_reg();
                twi_transmit_last_byte();
                break;
            case TWI_SELF_REG_ADC_PER:
                temp_w = adc_get_sample_per();
                TWI0.SDATA = (temp_w >> 8); // transmit MSB of ADC period
                twi_continue_transaction();
                persist_b = temp_w; // LSB of ADC period
                twi_transaction_state = TWI_SELF_SECOND_READ;
                break;
            default:
                twi_abort_idle();
                break;
            }
            break;
        default:
            twi_handle_error(slave_state);
            break;
        }
        break;

    case TWI_SELF_SECOND_READ:
        switch(slave_state) {
        case TWI_DATA_R_ACK:
            /* LSB of register word */
            TWI0.SDATA = persist_b; // LSB of ADC period
            twi_transmit_last_byte();
            break;
        default:
            twi_handle_error(slave_state);
            break;
        }
        break;

    case TWI_SELF_WRITE:
        switch(slave_state) {
        case TWI_DATA_W_ACK:
        case TWI_DATA_W_NACK:
            /* LSB of register word */
            adc_set_sample_per(
                               (((uint16_t) persist_b) << 8) | TWI0.SDATA
                               ); // persist_b holds MSB of ADC period
            twi_wait_stop();
            break;
        default:
            twi_handle_error(slave_state);
            break;
        }
        break;

    case TWI_FINISH_READ:
        switch(slave_state) {
        case TWI_DATA_R_NACK:
            twi_wait_stop();
            break;
        default:
            twi_handle_error(slave_state);
            break;
        }
        break;

    case TWI_AWAIT_STOP:
        switch(slave_state) {
        case TWI_STOP_R_ACK:
        case TWI_STOP_R_NACK:
        case TWI_STOP_W_ACK:
        case TWI_STOP_W_NACK:
            TWI0.SCTRLB = TWI_SCMD_NOACT_gc;
            twi_wrapup_transaction();
            break;
        case TWI_ADDR_W_ACK:
        case TWI_ADDR_W_NACK:
            twi_start_transaction();
            break;
        default:
            twi_handle_error(slave_state);
            break;
    }
    break;

    default: // Don't know how we got here
        twi_handle_error(slave_state);
        spi_stop_transaction();
        break;
    } // state switch
}
