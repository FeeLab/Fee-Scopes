/*
 ## Cypress FX3 Camera Kit source file (sensor.c)
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

/* This file implements the I2C based driver for the EV76C541 image sensor used
   in the FX3 HD 720p camera kit.

   Please refer to the Aptina EV76C541 sensor datasheet for the details of the
   I2C commands used to configure the sensor.
 */

#include <cyu3system.h>
#include <cyu3os.h>
#include <cyu3dma.h>
#include <cyu3error.h>
#include <cyu3uart.h>
#include <cyu3i2c.h>
#include <cyu3types.h>
#include <cyu3gpio.h>
#include <cyu3utils.h>
#include "sensor.h"
#include "util.h"
#include "appi2c.h"
#include "auxiliary.h"

#define CONFIRM_TRIES      5

#define PLL_BYPASS

/* ATTiny20 registers */
#define REG_ATTINY_INIT    0xFF

/* Python 480 registers */
#define SENSOR_SOFT_RESET_ANALOG_VAL   0x0999
#define SENSOR_CHIP_ID_VAL             0x5004
#define SENSOR_SEQ_GEN_CONFIG_START    0x0001
#define SENSOR_SEQ_GEN_CONFIG_STOP     0x0001
#define SENSOR_ANALOG_GAIN_1X          0x00E1
#define SENSOR_ANALOG_GAIN_2X          0x00E4
#define SENSOR_ANALOG_GAIN_3_5X        0x0024
#define SENSOR_SYNC_CONFIG_EXPS_SYNC   0x037A
#define SENSOR_SYNC_CONFIG_EXPS_NOSYNC 0x0372
#define SENSOR_ROI_X_CONFIG_FULL       0xC900 // assumes granularity is 4
#define SENSOR_ROI_Y_CONFIG_FULL       0x9700

#define SENSOR_30FPS_MULT_TIMER          1000

#ifdef PLL_BYPASS
#define SENSOR_30FPS_FR_LENGTH           547
#define SENSOR_30FPS_MAX_EXPOSURE        546
#else
#define SENSOR_30FPS_FR_LENGTH          2214
#define SENSOR_30FPS_MAX_EXPOSURE       2213
#endif

#define SENSOR_INITIAL_SETTING_ANALOG_GAIN SENSOR_ANALOG_GAIN_2X
// Startup values

CyU3PReturnStatus_t
SensorIsOn (
            CyBool_t *isOn)
{
    CyU3PReturnStatus_t apiRetStatus;
    uint8_t sensor_ctrl_val = 0;
    uint16_t sensor_reset_val = 0;
    apiRetStatus = I2CRead (AUX_ADDR_RD, REG_SENSOR_CTRL, 1, &sensor_ctrl_val);
    if (sensor_ctrl_val & SENSOR_CTRL_POWER_bm) {
        I2CSensorConditionalRead (&apiRetStatus,
                                  SENSOR_ADDR_RD,
                                  SENSOR_REG_SOFT_RESET_ANALOG,
                                  &sensor_reset_val);
        if (apiRetStatus == CY_U3P_SUCCESS) {
            *isOn = sensor_reset_val != SENSOR_SOFT_RESET_ANALOG_VAL;
        }
    }
    return apiRetStatus;
}

/* EV76C541 sensor initialization sequence.

   Sensor_Configure_Serdes: configure I2C bridge to CMOS board
   Sensor_Configure_EV76C541: configure EV76C541 on CMOS board

   Select Video Resolution
   SensorChangeConfig     : Update sensor configuration.
*/
CyU3PReturnStatus_t
SensorInit (
        void)
{
    CyU3PReturnStatus_t apiRetStatus;

    apiRetStatus = SensorConfigurePython480 (CyTrue, CyTrue);
    if (apiRetStatus == CY_U3P_SUCCESS) {
        CyU3PThreadSleep (50); // sleep for 50 ms
        /* Update sensor configuration based on desired video stream parameters.
         * Using 808x608 at 30fps as default setting.
         */

        apiRetStatus = SensorConfigureRoi1 ();
        if (apiRetStatus == CY_U3P_SUCCESS) {
            apiRetStatus = SensorScaling_808_608_30fps ();
            if (apiRetStatus == CY_U3P_SUCCESS) {
                apiRetStatus = SensorSetGain (SENSOR_INITIAL_SETTING_ANALOG_GAIN);
                if (apiRetStatus == CY_U3P_SUCCESS) {
                    apiRetStatus = ScopeAdcStart (CyTrue, 0x0034);
                }
            }
        }
    }
    return apiRetStatus;
}

CyU3PReturnStatus_t
SensorStart (
             void)
{
    CyU3PReturnStatus_t apiRetStatus;
    apiRetStatus = I2CSensorWrite (SENSOR_ADDR_WR,
                                   SENSOR_REG_SEQ_GEN_CONFIG,
                                   SENSOR_SEQ_GEN_CONFIG_START);
    return apiRetStatus;
}

CyU3PReturnStatus_t
SensorStop (
            void)
{
    CyU3PReturnStatus_t apiRetStatus;
    apiRetStatus = I2CSensorWrite (SENSOR_ADDR_WR,
                                   SENSOR_REG_SEQ_GEN_CONFIG,
                                   SENSOR_SEQ_GEN_CONFIG_STOP);
    return apiRetStatus;
}

CyU3PReturnStatus_t
SensorDisable (
             void)
{
    CyU3PReturnStatus_t apiRetStatus;
    CyBool_t isOn = CyFalse;
    apiRetStatus = SensorIsOn (&isOn);
    if (apiRetStatus == CY_U3P_SUCCESS && isOn) {
        apiRetStatus = SensorConfigurePython480 (CyFalse, CyFalse); // turn off sensor
        if (apiRetStatus == CY_U3P_SUCCESS) {
            CyU3PThreadSleep (20); // sleep for 20 ms to wait for power down
        }
    }
    return apiRetStatus;
}

/*
 * Verify that the sensor can be accessed over the I2C bus from FX3.
 */
CyU3PReturnStatus_t
SensorI2CBusTest (
        CyBool_t *success)
{
    /* The sensor ID register can be read here to verify sensor connectivity. */
    CyU3PReturnStatus_t apiRetStatus;
    uint16_t chip_id = 0;

    /* Reading sensor ID */
    apiRetStatus = I2CSensorRead (SENSOR_ADDR_RD, SENSOR_REG_CHIP_ID, &chip_id);
    if (apiRetStatus == CY_U3P_SUCCESS)
    {
        *success = chip_id == SENSOR_CHIP_ID_VAL;
    }
    return apiRetStatus;
}

CyU3PReturnStatus_t
SensorConfigureRoi1(
            void)
{
    CyU3PReturnStatus_t apiRetStatus;

    apiRetStatus =  I2CSensorWrite (SENSOR_ADDR_WR,
                                    SENSOR_REG_ROI0_X_CONFIG,
                                    SENSOR_ROI_X_CONFIG_FULL);
    I2CSensorConditionalWrite (&apiRetStatus,
                               SENSOR_ADDR_WR,
                               SENSOR_REG_ROI0_Y_CONFIG,
                               SENSOR_ROI_Y_CONFIG_FULL);
    return apiRetStatus;
}

/*
   The procedure is adapted from Aptina's sensor initialization scripts. Please
   refer to the EV76C541 sensor datasheet for details.
 */
CyU3PReturnStatus_t
SensorScaling_808_608_30fps (
        void)
{
    CyU3PReturnStatus_t apiRetStatus;
    uint16_t regvalue;
    I2CSensorRead (SENSOR_ADDR_RD,
        		SENSOR_REG_MULT_TIMER0,
        		&regvalue
        		);

    I2CSensorRead (SENSOR_ADDR_RD,
    		SENSOR_REG_FR_LENGTH0,
    		&regvalue
    		);

    I2CSensorRead (SENSOR_ADDR_RD,
        		SENSOR_REG_EXPOSURE0,
        		&regvalue
        		);
    /* Stop updating image sensor until all values are written */
    apiRetStatus =  I2CSensorWrite (SENSOR_ADDR_WR,
                                    SENSOR_REG_SYNC_CONFIG,
                                    SENSOR_SYNC_CONFIG_EXPS_NOSYNC);

    /*
       Video configuration
       Oscillator frequency is 66.66 MHz, period 15 ns
       PLL is enabled at 4x osc frequency, 266.66 MHz and 4 ns
       set mult_timer to 1000 to get period of 4 us for timing registers
       The actual frame length and exposure length was calculated using the ON
       semi exposure calculator
    */

    I2CSensorConditionalWrite (&apiRetStatus,
                               SENSOR_ADDR_WR,
                               SENSOR_REG_MULT_TIMER0,
                               SENSOR_30FPS_MULT_TIMER);

    I2CSensorConditionalWrite (&apiRetStatus,
                               SENSOR_ADDR_WR,
                               SENSOR_REG_FR_LENGTH0,
                               SENSOR_30FPS_FR_LENGTH);

    I2CSensorConditionalWrite (&apiRetStatus,
                               SENSOR_ADDR_WR,
                               SENSOR_REG_EXPOSURE0,
                               SENSOR_30FPS_MAX_EXPOSURE);

    /* Resume updating image sensor */
    I2CSensorConditionalWrite (&apiRetStatus,
                               SENSOR_ADDR_WR,
                               SENSOR_REG_SYNC_CONFIG,
                               SENSOR_SYNC_CONFIG_EXPS_SYNC);

    I2CSensorRead (SENSOR_ADDR_RD,
            		SENSOR_REG_MULT_TIMER0,
            		&regvalue
            		);

    I2CSensorRead (SENSOR_ADDR_RD,
    		SENSOR_REG_FR_LENGTH0,
    		&regvalue
    		);

    I2CSensorRead (SENSOR_ADDR_RD,
        		SENSOR_REG_EXPOSURE0,
        		&regvalue
        		);

    return apiRetStatus;
}

/*
   Get the current gain setting from the Python 480 sensor.
 */
CyU3PReturnStatus_t
SensorGetGain (
        uint8_t *translated_gain)
{
    CyU3PReturnStatus_t apiRetStatus;
    uint16_t sensor_gain = 0;

    apiRetStatus = I2CSensorRead (SENSOR_ADDR_RD,
                                  SENSOR_REG_ANALOG_GAIN,
                                  &sensor_gain);
    if (apiRetStatus == CY_U3P_SUCCESS) {
        switch (sensor_gain) {
        case SENSOR_ANALOG_GAIN_1X:
            *translated_gain = SENSOR_A_GAIN_TRANSL_1X;
            break;
        case SENSOR_ANALOG_GAIN_2X:
            *translated_gain = SENSOR_A_GAIN_TRANSL_2X;
            break;
        case SENSOR_ANALOG_GAIN_3_5X:
            *translated_gain = SENSOR_A_GAIN_TRANSL_3_5X;
            break;
        default:
            *translated_gain = 0;
            break;
        }
    }
    return apiRetStatus;
}

/*
   Update the gain setting for the Python 480 sensor.
 */
CyU3PReturnStatus_t
SensorSetGain (
        uint8_t new_translated_gain)
{
  uint16_t new_sensor_gain;
    CyU3PReturnStatus_t apiRetStatus;

    switch (new_translated_gain) {
            case SENSOR_A_GAIN_TRANSL_1X:
              new_sensor_gain = SENSOR_ANALOG_GAIN_1X;
                break;
            case SENSOR_A_GAIN_TRANSL_3_5X:
              new_sensor_gain = SENSOR_ANALOG_GAIN_3_5X;
                break;
            case SENSOR_A_GAIN_TRANSL_2X:
                // Fall-through
            default:
              new_sensor_gain = SENSOR_ANALOG_GAIN_2X;
                break;
            }
    apiRetStatus = I2CSensorWrite (SENSOR_ADDR_WR,
                                   SENSOR_REG_ANALOG_GAIN,
                                   new_sensor_gain);
    return apiRetStatus;
}
