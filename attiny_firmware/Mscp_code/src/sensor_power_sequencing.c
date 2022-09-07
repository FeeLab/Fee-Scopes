#include "sensor_power_sequencing.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "spi_master.h"
#include "python480_info.h"

/***************************************************
 * STORAGE FOR EXTERNALLY VISIBLE GLOBAL VARIABLES *
 ***************************************************/

volatile sensor_req_power_change_t SENSOR_POWER_REQ = NO_POWER_REQ;
volatile bool SENSOR_CONFIG_REQ = false;

/*************
 * CONSTANTS *
 *************/

#ifdef PLL_BYPASS

// pll bypass without adjustable exposure
#define DEF_EXPOSURE0   546
#define DEF_FR_LENGTH_0 547

#else // PLL_BYPASS

// pll used without adjustable exposure
#define DEF_EXPOSURE0   2214
#define DEF_FR_LENGTH_0 2213

#endif // PLL_BYPASS

/* Rationale for periods
 *
 * The main clock is assumed to be running at 5 MHz, so each clock cycle should
 * be 200 ns There should be 10 us between each step in the power sequence,
 * which is 50 clock cycles. This is a small enough number that polling the
 * timer will be better than using interrupts. The single shot mode of TCB0 may
 * be a good fit to time out the 10 us pauses between steps.
 *
 * PA4 - v3p3 EN
 * PA5 - VPIX EN
 * PA6 - CLK EN
 * PA7 - RESETN EN
 *
 * Need to raise v3p3, vpix, clk, then reset with 10 us between each The 3v3 and
 * vpix lines use the ON Semi NCP163AFCT330T2G regulator, which have a 120 us
 * turn on time, and 120 us turn off time The oscillator (ASD-66.66MHZ-LCS-T)
 * requires a MAXIMUM of 10 ms, 6 ms is typical.
 */

#define MIN_POWER_SEQUENCE_CYCLES 0x0032 // 10 us
#define REG_ENABLE_CYCLES         0x0258 // 120 us
#define OSC_ENABLE_CYCLES         0xC350 // 10 ms

#define CONFIG_DELAY_MS           10
/************
 * TYPEDEFS *
 ************/

/* enum sensor_state_t
 *
 * This enum represents the state of the python480, as the attiny816 currently
 * understands it. Many operations on the python480 require wait time between
 * steps, and as a consequence the attiny816 uses its timer B to measure out
 * these pauses.
 */
typedef enum {
              /* SENSOR_OFF
               * The sensor is off
               * reset asserted, only 1v8 power is supplied to sensor
               * TCB0 disabled, CCNT is undefined
               */
              SENSOR_OFF,

              /* SENSOR_PUP_COLDBOOT
               * Waiting for 1v8 to be stable
               * reset asserted
               * TCB0 enabled, CCNT is MIN_POWER_SEQUENCE_CYCLES
               */
              SENSOR_PUP_COLDBOOT,

              /* SENSOR_PUP_3V3_EN
               * Waiting for 3v3 to be stable
               * reset and 3v3 enable asserted
               * TCB0 enabled, CCNT is REG_ENABLE_CYCLES
               */
              SENSOR_PUP_3V3_EN,

              /* SENSOR_PUP_VPIX_EN
               * Waiting for vpix to be stable
               * reset, 3v3 and vpix enable asserted
               * TCB0 enabled, CCNT is REG_ENABLE_CYCLES
               */
              SENSOR_PUP_VPIX_EN,

              /* SENSOR_PUP_OSC_EN
               * Waiting for oscillator to be stable
               * reset, 3v3, vpix, and osc enable asserted
               * TCB0 enabled, CCNT is OSC_ENABLE_CYCLES
               */
              SENSOR_PUP_OSC_EN,

              /* SENSOR_PUP_RESET_OFF
               * Waiting for the image sensor to be ready for SPI
               * 3v3, vpix, and osc enable asserted, reset de-asserted
               * TCB0 enabled, CCNT is MIN_POWER_SEQUENCE_CYCLES
               */
              SENSOR_PUP_RESET_OFF,

              /* SENSOR_ON
               * The sensor is on and ready to receive SPI
               * 3v3, vpix, and osc enable asserted. Reset de-asserted, ready
               * for SPI. TCB0 disabled, CCNT is MIN_POWER_SEQUENCE_CYCLES
               */
              SENSOR_ON,

              /* SENSOR_CONFIGURED
               * The sensor is on and configured
               * TCB0 is disabled
               */
              SENSOR_CONFIGURED,

              /* SENSOR_PDOWN_RESET_ON
               * Waiting for the image sensor to go into reset
               * reset, 3v3, vpix, and osc enable asserted
               * TCB0 enabled, CCNT is MIN_POWER_SEQUENCE_CYCLES
               */
              SENSOR_PDOWN_RESET_ON,

              /* SENSOR_PDOWN_OSC_DIS
               * Waiting for the oscillator to turn off
               * reset, 3v3, and vpix asserted
               * TCB0 enabled, CCNT is OSC_ENABLE_CYCLES
               */
              SENSOR_PDOWN_OSC_DIS,

              /* SENSOR_PDOWN_VPIX_DIS
               * Waiting for vpix to turn off
               * reset and 3v3 asserted
               * TCB0 enabled, CCNT is REG_ENABLE_CYCLES
               */
              SENSOR_PDOWN_VPIX_DIS
} sensor_state_t;

/*******************
 * GLOBAL VARIABLES *
 ********************/

static volatile sensor_state_t current_sensor_state = SENSOR_OFF;

/*************************
 * FUNCTION DECLARATIONS *
 *************************/

/* void set_new_period(uint16_t new_per)
 *
 * Sets a new timer period and enables TCB0. TCB0 should be disabled before
 * calling.
 */
static inline void set_new_period(uint16_t new_per);

/* void reset_period(uint16_t new_per)
 *
 * Sets a new timer period and enables TCB0. TCB0 should be enabled before
 * calling. Unlike `set_new_period`, this assumes that TCB0 is already enabled.
 */
static inline void reset_period(uint16_t new_per);

/* void sensor_setpin_time(uint8_t pinmask);
 *
 * Asserts the pin(s) specified by `pinmask`, and then starts TCB0.
 */
static inline void sensor_setpin_time(uint8_t pinmask);

/* void sensor_setpin_time(uint8_t pinmask);
 *
 * De-asserts the pin(s) specified by `pinmask`, and then starts TCB0.
 */
static inline void sensor_clearpin_time(uint8_t pinmask);

/* static void sensor_start_powerup(void)
 *
 * Starts the sensor power up sequence
 * Assumes current power state is SENSOR_OFF
 */
static void sensor_start_powerup(void);

/* static void sensor_start_powerdown(void)
 *
 * Starts the sensor power down sequence. Assumes current power state is
 * SENSOR_ON or SENSOR_CONFIGURED
 */
static void sensor_start_powerdown(void);

/* void sensor_enable_clk_mngmnt_1(void)
 *
 * Enables the clock on the Python480, corresponding to the step of the same
 * name described in the Python480 datasheet.
 *
 * Assumes SPI mutex is held.
 */
static void sensor_enable_clk_mngmt_1(void);

/* void sensor_enable_clk_mngmnt_2(void)
 *
 * Distributes the clock signal and enables the logic on the Python480,
 * corresponding to the step of the same name described in the Python480
 * datasheet.
 *
 * Assumes SPI mutex is held.
 */
static void sensor_enable_clk_mngmt_2(void);

/* void sensor_spi_upload(void)
 *
 * Configures the Python480 and enables the sequencer.
 *
 * Assumes SPI mutex is held.
 */
static void sensor_spi_upload(void);

/* void sensor_spi_program_upload(void)
 *
 * Perform required program upload to Python480.
 *
 * Assumes SPI mutex is held.
 */
static void sensor_spi_program_upload(void);

/* void sensor_soft_powerup(void)
 *
 * Takes the Python 480 out of soft-reset.
 *
 * Assumes SPI mutex is held.
 */
static void sensor_soft_powerup(void);

/* bool sensor_req_configuration(void)
 *
 * Requests that the image sensor be configured. If the current state of the
 * image sensor is `SENSOR_ON` then this function will start the process of
 * configuring the image sensor over SPI, and return true. If it is instead
 * `SENSOR_CONFIGURED`, then it will do nothing and return true. Otherwise, it
 * will return false.
 *
 * This configuration process is started with the function
 * `sensor_req_configuration`. This process can be monitored by checking the
 * output of `sensor_is_configured` after starting the configuration process.
 * The function `sensor_is_configured` will not return true until configuration
 * has been completed successfully.
 *
 * Uses SPI system
 */
static bool sensor_req_configuration(void);

/* static void configure_sensor(void)
 *
 * Uses SPI system
 */
static void configure_sensor(void);

/*****************************************
 * EXTERN INLINE DECLARATIONS FOR LINKER *
 *****************************************/

extern inline void sequencing_enable_interrupts(void);
extern inline void sequencing_disable_interrupts(void);

/************************
 * FUNCTION DEFINITIONS *
 ************************/

void sensor_sequencing_init(void)
{
    /* Sequencing will use timer B0 and port A pins 4-7 to turn on
     * different enable lines in sequence
     */

    // Configure output pins 4-7
    // Disable input buffers for port A pin 4-7
    PORTA.PIN4CTRL = PORTA.PIN5CTRL = PORTA.PIN6CTRL = PORTA.PIN7CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTA.OUTCLR = 0xF0; // Set the output of pins 4-7 to low
    PORTA.DIRSET = 0xF0; // Set the direction of pins 4-7 to out

    // Configure event system
    EVSYS.ASYNCUSER0 = EVSYS_ASYNCUSER0_SYNCCH1_gc;

    // Configure TCB0
    TCB0.CTRLB       = TCB_CNTMODE_SINGLE_gc;
    TCB0.EVCTRL      = TCB_CAPTEI_bm;
    TCB0.CNT         = MIN_POWER_SEQUENCE_CYCLES;
    TCB0.CCMP        = MIN_POWER_SEQUENCE_CYCLES;

    sequencing_enable_interrupts();
}

static inline void set_new_period(uint16_t new_per)
{
    TCB0.CNT = new_per;
    TCB0.CCMP = new_per;
    TCB0.CTRLA = TCB_ENABLE_bm; // enable_timer
}

static inline void reset_period(uint16_t new_per)
{
    TCB0.CTRLA = 0x00; // disable timer to change config
    set_new_period(new_per);
}

static inline void sensor_setpin_time(uint8_t pinmask)
{
    PORTA.OUTSET = pinmask; // set pin
    EVSYS.SYNCSTROBE = 0x02; // start timer
}

static inline void sensor_clearpin_time(uint8_t pinmask)
{
    PORTA.OUTCLR = pinmask; // set pin
    EVSYS.SYNCSTROBE = 0x02; // start timer
}

static void sensor_start_powerup(void)
{
    static bool cold_boot = true; // only true for the first function call
    if (cold_boot) {
        cold_boot = false;
        set_new_period(MIN_POWER_SEQUENCE_CYCLES);
        TCB0.CTRLA = TCB_ENABLE_bm; // turn on TCB0
        EVSYS.SYNCSTROBE = 0x02; // start timer
        current_sensor_state = SENSOR_PUP_COLDBOOT;
    } else {
        set_new_period(REG_ENABLE_CYCLES);
        TCB0.CTRLA = TCB_ENABLE_bm; // turn on TCB0
        sensor_setpin_time(PIN4_bm);
        current_sensor_state = SENSOR_PUP_3V3_EN;
    }
}

static void sensor_start_powerdown(void)
{
    set_new_period(MIN_POWER_SEQUENCE_CYCLES);
    TCB0.CTRLA = TCB_ENABLE_bm; // turn on TCB0
    sensor_clearpin_time(PIN7_bm);
    current_sensor_state = SENSOR_PDOWN_RESET_ON;
}

bool sensor_req_powerup(void)
{
    bool success = false;
    if (sensor_is_off()) {
        sensor_start_powerup();
        success = true;
    } else {
        success = false;
    }
    return success;
}

bool sensor_req_powerdown(void)
{
    bool success = false;
    if (sensor_is_on()) {
        sensor_start_powerdown();
        success = true;
    } else {
        success = false;
    }
    return success;
}

void sensor_process_power_reqs(void) {
    switch (SENSOR_POWER_REQ) {
    case REQ_POWER_UP:
        SENSOR_POWER_REQ = NO_POWER_REQ;
        sensor_req_powerup();
        break;
    case REQ_POWER_DOWN:
        SENSOR_POWER_REQ = NO_POWER_REQ;
        sensor_req_powerdown();
        break;
    default:
        break;
    }
}

bool sensor_is_on(void)
{
    bool res;
    switch(current_sensor_state) {
    case SENSOR_ON:
    case SENSOR_CONFIGURED:
        res = true;
        break;
    default:
        res = false;
        break;
    }
    return res;
}

bool sensor_is_off(void)
{
    return current_sensor_state == SENSOR_OFF;
}

bool sensor_is_changing_power(void)
{
    return (! sensor_is_on()) & (! sensor_is_off());
}

bool sensor_is_configured(void)
{
    return current_sensor_state == SENSOR_CONFIGURED;
}


/******************************************************************************/

/*************************************************************************
 * THIS SECTION CONTAINS INFORMATION PROTECTED BY NDA WITH ARROW.		*
 * CONTACT A PYTHON DISTRIBUTOR TO ARRANGE NDA FOR FULL ACCESS			*
 * TO PROGRAMMING INFORMATION.											*
 *************************************************************************/

static void sensor_enable_clk_mngmt_1(void)
{
   // REDACTED
}

static void sensor_enable_clk_mngmt_2(void)
{
    // REDACTED
}

static void sensor_spi_upload(void)
{
	// REDACTED
}

static void sensor_spi_program_upload(void)
{
    // REDACTED
}

static void sensor_soft_powerup(void)
{
    // REDACTED
}

/*********************************
 * END OF SECTION COVERED BY NDA *
 *********************************/

/******************************************************************************/

static void configure_sensor(void)
{
    sensor_enable_clk_mngmt_1();
    _delay_ms(CONFIG_DELAY_MS);
    sensor_enable_clk_mngmt_2();
    sensor_spi_upload();
    sensor_spi_program_upload();
    sensor_soft_powerup();
    current_sensor_state = SENSOR_CONFIGURED;
}

static bool sensor_req_configuration(void)
{
    bool res;
    switch(current_sensor_state) {
    case SENSOR_ON:
        configure_sensor();
        // fall through
    case SENSOR_CONFIGURED:
        res = true;
        break;
    default:
        res = false;
        break;
    }
    return res;
}

void sensor_process_config_reqs(void) {
    if (SENSOR_CONFIG_REQ) {
        if (sensor_is_off() && SENSOR_POWER_REQ == NO_POWER_REQ) {
            // invalid request state
            SENSOR_CONFIG_REQ = false;
        } else if (sensor_is_on()) {
            if (spi_trylock_atomic(SPI_MAIN)) {
                SENSOR_CONFIG_REQ = false;
                sensor_req_configuration();
                spi_tryunlock_atomic(SPI_MAIN);
            }
        }
    }
}

/********
 * ISRS *
 ********/

// This needs the SPI bus in some cases
ISR(TCB0_INT_vect)
{
    TCB0.INTFLAGS = TCB_CAPT_bm; // clear interrupt flag
    switch(current_sensor_state) {

    case SENSOR_PUP_COLDBOOT:
        reset_period(REG_ENABLE_CYCLES);
        sensor_setpin_time(PIN4_bm); // turn on 3v3
        current_sensor_state = SENSOR_PUP_3V3_EN;
        break;

    case SENSOR_PUP_3V3_EN:
        sensor_setpin_time(PIN5_bm); // turn on vpix
        current_sensor_state = SENSOR_PUP_VPIX_EN;
        break;

    case SENSOR_PUP_VPIX_EN:
        // Prepare timer for oscillator turn-on period
        reset_period(OSC_ENABLE_CYCLES);
        sensor_setpin_time(PIN6_bm); // turn on oscillator
        current_sensor_state = SENSOR_PUP_OSC_EN;
        break;

    case SENSOR_PUP_OSC_EN:
        // Prepare timer for reset time
        reset_period(MIN_POWER_SEQUENCE_CYCLES);
        sensor_setpin_time(PIN7_bm); // turn off reset
        current_sensor_state = SENSOR_PUP_RESET_OFF;
        break;

    case SENSOR_PUP_RESET_OFF:
        TCB0.CTRLA = 0x00; // disable TCB0
        current_sensor_state = SENSOR_ON;
        break;

    case SENSOR_PDOWN_RESET_ON:
        // Prepare timer for oscillator turn-off period
        reset_period(OSC_ENABLE_CYCLES);
        sensor_clearpin_time(PIN6_bm); // turn off oscillator
        current_sensor_state = SENSOR_PDOWN_OSC_DIS;
        break;

    case SENSOR_PDOWN_OSC_DIS:
        // Prepare timer for regulator turn-off period
        reset_period(REG_ENABLE_CYCLES);
        sensor_clearpin_time(PIN5_bm); // turn off vpix
        current_sensor_state = SENSOR_PDOWN_VPIX_DIS;
        break;

    case SENSOR_PDOWN_VPIX_DIS:
        // Prepare timer for regulator turn-off period
        sensor_clearpin_time(PIN4_bm); // turn off 3v3
        TCB0.CTRLA = 0x00; // disable TCB0
        current_sensor_state = SENSOR_OFF;
        break;

    case SENSOR_ON:
    case SENSOR_CONFIGURED:
    case SENSOR_OFF:
    default:
        // This case should not happen!
        TCB0.INTFLAGS = TCB_CAPT_bm; // clear interrupt flag
        TCB0.CTRLA = 0x00; // disable TCB0
        break;
    }
}
