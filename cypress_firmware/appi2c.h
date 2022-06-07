#ifndef _INCLUDED_APP_I2C_H_
#define _INCLUDED_APP_I2C_H_

#include <cyu3types.h>

#define I2C_SLAVEADDR_MASK 0xFE         /* Mask to get actual I2C slave address value without direction bit. */

extern CyU3PReturnStatus_t
I2CWriteConfirm2B (
		uint8_t slaveAddr,
		uint8_t regAddr,
		uint8_t highData,
		uint8_t lowData,
		uint32_t retryCount);

extern void
CyFxUVCApplnI2CInit (void);

/* Function    : SensorWrite2B
   Description : Write two bytes of data to image sensor over I2C interface.
   Parameters  :
                 slaveAddr - I2C slave address for the sensor.
                 regAddr  - Memory address being written to.
                 highData  - High byte of data to be written.
                 lowData   - Low byte of data to be written.
 */
extern CyU3PReturnStatus_t
I2CWrite2B (
        uint8_t slaveAddr,
        uint8_t regAddr,
        uint8_t highData,
        uint8_t lowData);

/* Function    : I2CWrite
   Description : Write arbitrary amount of data to image sensor over I2C interface.
   Parameters  :
                 slaveAddr - I2C slave address for the sensor.
                 regAddr   - Memory address being written to.
                 lowAddr   - Low byte of memory address being written to.
                 count     - Size of write data in bytes. Limited to a maximum of 64 bytes.
                 buf       - Pointer to buffer containing data.
 */
extern CyU3PReturnStatus_t
I2CWrite (
        uint8_t slaveAddr,
        uint8_t regAddr,
        uint8_t count,
        uint8_t *buf);

CyU3PReturnStatus_t
I2CWriteNoReg (
        uint8_t slaveAddr,
        uint8_t data);

/* Function    : I2CRead2B
   Description : Read 2 bytes of data from image sensor over I2C interface.
   Parameters  :
                 slaveAddr - I2C slave address for the sensor.
                 regAddr   - Memory address being written to.
                 lowAddr   - Low byte of memory address being written to.
                 buf       - Buffer to be filled with data. MSB goes in byte 0.
 */
extern CyU3PReturnStatus_t
I2CRead2B (
        uint8_t slaveAddr,
        uint8_t regAddr,
        uint8_t *buf);

/* Function    : I2CRead
   Description : Read arbitrary amount of data from image sensor over I2C interface.
   Parameters  :
                 slaveAddr - I2C slave address for the sensor.
                 regAddr   - Memory address being written to.
                 lowAddr   - Low byte of memory address being written to.
                 count     - Size of data to be read in bytes. Limited to a max of 64.
                 buf       - Buffer to be filled with data.
 */
extern CyU3PReturnStatus_t
I2CRead (
        uint8_t slaveAddr,
        uint8_t regAddr,
        uint8_t count,
        uint8_t *buf);

/* Function    : I2CReadNoReg
   Description : Read one byte of data from sensor over I2C interface.
   Parameters  :
                 slaveAddr - I2C slave address for the sensor.
                 buf       - Buffer to be filled with one byte of data
 */
CyU3PReturnStatus_t
I2CReadNoReg (
        uint8_t slaveAddr,
        uint8_t *buf);

void
I2CSensorConditionalWrite (
                           CyU3PReturnStatus_t *apiRetStatus,
                           uint8_t slaveAddr,
                           uint16_t regAddr,
                           uint16_t regValue);

CyU3PReturnStatus_t
I2CSensorWrite (
                uint8_t slaveAddr,
                uint16_t regAddr,
                uint16_t regValue);

CyU3PReturnStatus_t
I2CSensorRead (
               uint8_t slaveAddr,
               uint16_t regAddr,
               uint16_t *buf);

void
I2CSensorConditionalRead (
                          CyU3PReturnStatus_t *apiRetStatus,
                          uint8_t slaveAddr,
                          uint16_t regAddr,
                          uint16_t *buf);
#endif
