/*
 * Mscp_test.c
 *
 * Created: 6/12/2017 5:06:03 PM
 * Author : Galen Lynch and Michale Fee
 */

#include <stdbool.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/power.h>
#include <avr/sleep.h>

#include "spi_master.h"
#include "twi_slave.h"
#include "usart_out.h"
#include "adc.h"
#include "sensor_power_sequencing.h"
#include "python480_info.h"

#ifdef DEBUG_REGISTER_SET_GET
#undef ALLOW_SLEEP
#endif

/************************
 * FUNCTION DEFINITIONS *
 ************************/

static void clk_init(void)
{
    CPU_CCP = CCP_IOREG_gc;
    CLKCTRL.MCLKCTRLB = CLKCTRL_PDIV_4X_gc | CLKCTRL_PEN_bm; // Set main clock frequency to 5 MHz
}

// Turn off unused peripherals
static inline void device_cleanup(void)
{
    PORTB.PIN6CTRL = PORTB.PIN7CTRL = PORT_ISC_INPUT_DISABLE_gc; // not connected to package
    PORTC.PIN4CTRL = PORTC.PIN5CTRL = PORT_ISC_INPUT_DISABLE_gc; // not connected to package
}

int main(void)
{

#ifdef DEBUG_REGISTER_SET_GET
    volatile uint16_t regval = 0;
    volatile bool check_registers = false;
#endif

    clk_init();
    twi_init();
    twi_enable(); // I2C clock stretching starting here
    twi_enable_interrupts();
    sensor_sequencing_init();
    spi_init();
    adc_init();
    sei(); // respond to I2C starting here
    device_cleanup();

#ifdef ALLOW_SLEEP
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
#endif

#ifdef DEBUG_STARTUP_SENSOR
    sensor_req_powerup();
    SENSOR_CONFIG_REQ = true;
#endif

#if defined DEBUG_ADC || defined DEBUG_ADC_MULTIPLEX
    ADC_SAMPLE_REQ = true;
#ifdef DEBUG_ADC_MULTIPLEX
    ADC_MULTIPLEX_REQ = true;
#endif
#endif

    while(true) {
        sensor_process_power_reqs();
        sensor_process_config_reqs();
        adc_process_reqs();

#ifdef DEBUG_REGISTER_SET_GET
        if (sensor_is_configured() && check_registers) {
            if (spi_trylock_atomic(SPI_MAIN)) {
                spi_python480_write(SENSOR_REG_EXPOSURE0, regval);
                regval = spi_python480_read(SENSOR_REG_EXPOSURE0);

                spi_tryunlock_atomic(SPI_MAIN);
            }
        }
#endif

#ifdef ALLOW_SLEEP
        cli();
        if (!(
              /* Pending requests */
              SENSOR_POWER_REQ != NO_POWER_REQ ||
              SENSOR_CONFIG_REQ ||
              ADC_SAMPLE_REQ ||
              ADC_UPDATE_REQ ||
              /* Active states */
              sensor_is_changing_power() ||
              twi_in_transaction()
              )) {
                sleep_enable();
                sei();
                sleep_cpu();
                sleep_disable();
        }
        sei();
#endif

    } // end main loop
}
