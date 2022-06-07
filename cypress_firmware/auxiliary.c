/*
 * auxiliary.c
 *
 *  Created on: Sep 14, 2017
 *      Author: Galen
 */

#include "auxiliary.h"
#include "appi2c.h"
#include "util.h"
#include <cyu3types.h>
#include <cyu3error.h>

#define REG_ADC_CTRL    0x01
#define REG_ADC_PERIOD  0x02

CyU3PReturnStatus_t
SensorConfigurePython480 (
    CyBool_t config_req,
    CyBool_t power_req) //SPI configuration of sensor
{
    CyU3PReturnStatus_t apiRetStatus;
    uint8_t ctrl_val = 0x00;
    if (config_req) {
        ctrl_val |= SENSOR_CTRL_CONFIGURE_bm;
    }
    if (power_req) {
        ctrl_val |= SENSOR_CTRL_POWER_bm;
    }
    apiRetStatus = I2CWrite (AUX_ADDR_WR, REG_SENSOR_CTRL, 1, &ctrl_val);
    return apiRetStatus;
}

CyU3PReturnStatus_t
SensorReadCtrlReg (
                   uint8_t *status
)
{
    CyU3PReturnStatus_t apiRetStatus;
    apiRetStatus = I2CRead (AUX_ADDR_WR, REG_SENSOR_CTRL, 1, status);
    return apiRetStatus;
}

/*
   Start taking and transmitting ADC reads
 */
CyU3PReturnStatus_t
ScopeAdcStart (
    CyBool_t multiplex_req,
    uint16_t adc_per)
{
    CyU3PReturnStatus_t apiRetStatus;
    uint8_t buf[2];
    FillBuff2B (adc_per, buf);
    apiRetStatus = I2CWrite2B (AUX_ADDR_WR, REG_ADC_PERIOD, buf[0], buf[1]);
    buf[0] = multiplex_req ? 0x03 : 0x01;
    if (apiRetStatus == CY_U3P_SUCCESS) {
        apiRetStatus = I2CWrite (AUX_ADDR_WR, REG_ADC_CTRL, 1, buf);
    }
    return apiRetStatus;
}

/*
   Get current ADC period on scope
 */
CyU3PReturnStatus_t
ScopeGetAdcPeriod (
    uint16_t *period)
{
    CyU3PReturnStatus_t apiRetStatus;
    uint8_t buf[2];
    apiRetStatus = I2CRead2B (AUX_ADDR_RD, REG_ADC_PERIOD, buf);
    *period = Combine2B (buf);
    return apiRetStatus;
}

/*
   Stop taking and transmitting ADC reads
 */
CyU3PReturnStatus_t
ScopeAdcStop (
        void)
{
    CyU3PReturnStatus_t apiRetStatus;
    uint8_t ctrl_val = 0x00;
    apiRetStatus = I2CWrite (AUX_ADDR_WR, REG_ADC_CTRL, 1, &ctrl_val);
    return apiRetStatus;
}
