/*
 * auxiliary.h
 *
 *  Created on: Sep 14, 2017
 *      Author: Galen
 */

#ifndef AUXILIARY_H_
#define AUXILIARY_H_

#include <cyu3types.h>

/* I2C address for the LED. */
#define AUX_ADDR_WR     0x98
#define AUX_ADDR_RD     0x99
#define AUX_ADC_MIN_PER 0x0034

#define REG_SENSOR_CTRL 0x00

#define SENSOR_CTRL_CONFIG_ACK_bp 0
#define SENSOR_CTRL_CONFIG_ACK_bm ( 1 << SELF_SENSOR_CONFIG_ACK_bp )
#define SENSOR_CTRL_CONFIGURE_bp 1
#define SENSOR_CTRL_CONFIGURE_bm ( 1 << SENSOR_CTRL_CONFIGURE_bp )
#define SENSOR_CTRL_POWER_ACK_bp 6
#define SENSOR_CTRL_POWER_ACK_bm ( 1 << SELF_SENSOR_POWER_ACK_bp )
#define SENSOR_CTRL_POWER_bp 7
#define SENSOR_CTRL_POWER_bm ( 1 << SENSOR_CTRL_POWER_bp )

/* Function    : SensorInit
   Description : Initialize the EV76C541 sensor.
   Parameters  :
                CyBool_t config_req - request configuration
                CyBool_t power_req - request sensor power on
*/
extern CyU3PReturnStatus_t
SensorReadCtrlReg (
                 uint8_t *status
                 );

extern CyU3PReturnStatus_t
SensorConfigurePython480 (
                          CyBool_t config_req,
                          CyBool_t power_req);

/* Function    : SensorCheckInit
   Description : Read the status of the image sensor
   Parameters  : uint8_t* status - location to write status
*/
CyU3PReturnStatus_t
SensorCheckInit (
                 uint8_t *status
                   );

/*
   Function    : ScopeAdcStart
   Description : Starting taking and transmitting ADC reads from scope
   Parameters  :
                 multiplex_req - true or false to multiplex.
                 adc_per - ADC period
 */
extern CyU3PReturnStatus_t
ScopeAdcStart (
    CyBool_t multiplex_req,
    uint16_t adc_per);

/*
   Function    : ScopeGetAdcPeriod
   Description : Get period of ADC on scope
   Parameters  : uint16_t *period - location to return result
   Returns     : CyU3PReturnStatus_t - status of I2C transaction
 */
extern CyU3PReturnStatus_t
ScopeGetAdcPeriod (
                   uint16_t *period);

/*
   Function    : ScopeAdcStop
   Description : Stop taking and transmitting ADC reads from scope
   Parameters  : None
 */
extern CyU3PReturnStatus_t
ScopeAdcStop (
        void);

#endif /* AUXILIARY_H_ */
