#ifndef SEQUENCING_H
#define SEQUENCING_H

#include <stdbool.h>
#include <avr/io.h>

/* Sensor power sequencing
 *
 * Use the timer and interrupts to step through a sequence of enabling power and
 * SPI uploads to bring the image sensor into a functional state. This process
 * is started by calling `sensor_req_powerup`. The sensor state is not shared
 * outside of this compilation unit, but can be accessed with functions such as
 * `sensor_is_on` and `sensor_is_off`. This same logic is used to track and wait
 * for the programming uploads to complete.
 *
 * With the settings at the time of this writing, the minimum time for the
 * sensor to power up is 0.01026 seconds, and the configuration will last 0.01
 * to 0.1 seconds, depending on how quickly the PLL locks. Thus, the total time
 * for the sensor to be in an operational state from turn on is 0.02026 to
 * 0.11026 seconds.
 *
 * Hardware used: TCB0, ASYNCUSER0, SYNCCH1, interrupts, PA4-7
 */


/************
 * TYPEDEFS *
 ************/

typedef enum {
              NO_POWER_REQ,
              REQ_POWER_UP,
              REQ_POWER_DOWN
} sensor_req_power_change_t;

/***************************************
 * EXTERNALLY VISIBLE GLOBAL VARIABLES *
 ***************************************/

/* sensor_req_power_change_t SENSOR_POWER_REQ
 *
 * Used to control the power-up and power-down of the Python480. This variable
 * is handled by `sensor_process_power_reqs`.
 */
extern volatile sensor_req_power_change_t SENSOR_POWER_REQ;

/* bool SENSOR_CONFIG_REQ
 *
 * Used to control the configuration of the Python480. This variable
 * is handled by `sensor_process_config_reqs`.
 */
extern volatile bool SENSOR_CONFIG_REQ;

/*******************************
 * INLINE FUNCTION DEFINITIONS *
 *******************************/

/* Enable and disable the timer that drives power sequencing */

inline void sequencing_enable_interrupts(void) {
    TCB0.INTCTRL = TCB_CAPT_bm;
}

inline void sequencing_disable_interrupts(void) {
    TCB0.INTCTRL = 0x00;
}

/*************************
 * FUNCTION DECLARATIONS *
 *************************/

/* Initialize hardware */
void sensor_sequencing_init(void);

/* Functions to observe the current state of the sensor */
bool sensor_is_on(void);
bool sensor_is_off(void);
bool sensor_is_changing_power(void);
bool sensor_is_configured(void);

/* bool sensor_req_powerup(void);
 *
 * Attempts to power-up the sensor, which will succeed if the sensor is off.
 * Returns a bool that indicates if the power-up was successfully started.
 */
bool sensor_req_powerup(void);

/* bool sensor_req_powerdown(void);
 *
 * Attempts to power-down the sensor, which will succeed if the sensor is on.
 * Returns a bool that indicates if the power-down was successfully started.
 */
bool sensor_req_powerdown(void);

/* void sensor_process_config_reqs(void)
 *
 * Will look at `SENSOR_CONFIG_REQ` and if true, will determine if the sensor is
 * ready to be configured. If it is, then will attempt to lock the SPI mutex,
 * and if successful, will program the sensor, and then release the mutex. Will
 * set `SENSOR_CONFIG_REQ` to false if the image sensor is either configured
 * successfully, or instead if the image sensor is off and there is not already
 * a pending power-up request.
 */
void sensor_process_power_reqs(void);

/* void sensor_process_config_reqs(void)
 *
 * Will look at `SENSOR_POWER_REQ` and if not `NO_POWER_REQ`, it will take the
 * appropriate action to try to change the power state of the image sensor.
 * `SENSOR_POWER_REQ` will be set to `NO_POWER_REQ` when the function finishes.
 */
void sensor_process_config_reqs(void);

#endif
