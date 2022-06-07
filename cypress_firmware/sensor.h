/*
 ## Cypress FX3 Camera Kit header file (sensor.h)
 ## ===========================
 ##
 ##  Copyright Cypress Semiconductor Corporation, 2010-2012,
 ##  All Rights Reserved
 ##  UNPUBLISHED, LICENSED SOFTWARE.
 ##
 ##  CONFIDENTIAL AND PROPRIETARY INFORMATION
 ##  WHICH IS THE PROPERTY OF CYPRESS.
 ##
 ##  Use of this file is governed
 ##  by the license agreement included in the file
 ##
 ##     <install>/license/license.txt
 ##
 ##  where <install> is the Cypress software
 ##  installation root directory path.
 ##
 ## ===========================
*/

/* This file defines the parameters and the interface for the EV76C541 image
   sensor driver.
 */

#ifndef _INCLUDED_SENSOR_H_
#define _INCLUDED_SENSOR_H_

#include <cyu3types.h>

/* The SADDR line allows EV76C541 image sensor to select between two different I2C slave address.
   If the SADDR line is high, enable this #define to allow access to the correct I2C address for the sensor.
 */

/* I2C Slave address for the image sensor. */
#define SENSOR_ADDR_WR 0xB8             /* Slave address used to write sensor registers. */
#define SENSOR_ADDR_RD 0xB9             /* Slave address used to read from sensor registers. */

/* GPIO 20 on FX3 is used as SYNC output -- high during FV */
#define SENSOR_SYNC_GPIO 20

#define SENSOR_REG_CHIP_ID             0
#define SENSOR_REG_SOFT_RESET_ANALOG  10
#define SENSOR_REG_SEQ_GEN_CONFIG    192
#define SENSOR_REG_MULT_TIMER0       199
#define SENSOR_REG_FR_LENGTH0        200
#define SENSOR_REG_EXPOSURE0         201
#define SENSOR_REG_ANALOG_GAIN       204
#define SENSOR_REG_SYNC_CONFIG       206
#define SENSOR_REG_ROI0_X_CONFIG     256
#define SENSOR_REG_ROI0_Y_CONFIG     257
#define SENSOR_REG_ROI_CONFIG_LSB0   264
#define SENSOR_REG_ROI_CONFIG_LSB1   265

#define SENSOR_A_GAIN_TRANSL_1X   1
#define SENSOR_A_GAIN_TRANSL_2X   2
#define SENSOR_A_GAIN_TRANSL_3_5X 3

/* Communication over saturation channel */
#define SATURATION_RECORD_START 0x01
#define SATURATION_RECORD_END 0x02
#define SATURATION_INIT     0x03
#define SATURATION_FPS5     0x11
#define SATURATION_FPS10    0x12
#define SATURATION_FPS15    0x13
#define SATURATION_FPS20    0x14
#define SATURATION_FPS30    0x15
#define SATURATION_FPS60    0x16

extern CyU3PReturnStatus_t
SensorConfigureRoi1(
                      void);

extern CyU3PReturnStatus_t
SensorInit (
            void);

extern CyU3PReturnStatus_t
SensorStart (
    void);

extern CyU3PReturnStatus_t
SensorStop (
    void);

/* Function    : SensorIsOn
   Description : Check to see if the sensor is on
   Parameters  : None
*/
extern CyU3PReturnStatus_t
SensorIsOn (
            CyBool_t *isOn);

extern CyU3PReturnStatus_t
SensorDisable (
        void);

/* Function     : SensorScaling_HD720p_30fps
   Description  : Configure the EV76C541 sensor for 720p 30 fps video stream.
   Parameters   : None
 */
extern CyU3PReturnStatus_t
SensorScaling_808_608_30fps (
        void);

/* Function    : SensorI2CBusTest
   Description : Test whether the EV76C541 sensor is connected on the I2C bus.
   Parameters  : None
 */
extern CyU3PReturnStatus_t
SensorI2CBusTest (
        CyBool_t *connected);

/* Function    : SensorGetGain
   Description : Get the current gain setting from the EV76C541 sensor.
   Parameters  : None
 */
extern CyU3PReturnStatus_t
SensorGetGain (
               uint8_t *translated_gain);

/* Function    : SensorSetGain
   Description : Set the desired gain setting on the EV76C541 sensor.
   Parameters  :
                 gain - Desired gain level.
 */
extern CyU3PReturnStatus_t
SensorSetGain (
        uint8_t new_translated_gain);

#endif /* _INCLUDED_SENSOR_H_ */

/*[]*/
