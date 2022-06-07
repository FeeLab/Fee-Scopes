#include <cyu3error.h>
#include <cyu3i2c.h>
#include <cyu3os.h>
#include <stdbool.h>
#include "appi2c.h"
#include "app_error_handler.h"
#include "util.h"

#define NRETRY 5

/* I2C initialization. */
void
CyFxUVCApplnI2CInit (void)
{
    CyU3PI2cConfig_t i2cConfig;;
    CyU3PReturnStatus_t status;

    status = CyU3PI2cInit ();
    if (status != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "I2C initialization failed!\n");
        CyFxAppErrorHandler (status);
    }

    /*  Set I2C Configuration */
    i2cConfig.bitRate    = 400000;      /*  400 KHz */
    i2cConfig.isDma      = CyFalse;
    i2cConfig.busTimeout = 0xFFFFBE6E0; // 0x0000BE6E0 30 ms timeout @ 26 MHz core freq
    i2cConfig.dmaTimeout = 0xFFFF;

    status = CyU3PI2cSetConfig (&i2cConfig, 0);
    if (CY_U3P_SUCCESS != status)
    {
        CyU3PDebugPrint (4, "I2C configuration failed!\n");
        CyFxAppErrorHandler (status);
    }
}

static CyU3PReturnStatus_t
RobustI2cTramsmitBytes (
    CyU3PI2cPreamble_t  *preamble,
    uint8_t *data,
    uint32_t count,
    uint32_t retryCount)
{
  CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
  uint32_t attempts = 1;
  apiRetStatus = CyU3PI2cTransmitBytes (preamble, data, count, 0);
  while (apiRetStatus != CY_U3P_SUCCESS && attempts <= retryCount) {
    CyU3PI2cDeInit ();
    CyFxUVCApplnI2CInit ();
    apiRetStatus = CyU3PI2cTransmitBytes (preamble, data, count, 0);
    attempts++;
  }
  return apiRetStatus;
}

static CyU3PReturnStatus_t
RobustI2cReceiveBytes (
    CyU3PI2cPreamble_t  *preamble,
    uint8_t *data,
    uint32_t count,
    uint32_t retryCount)
{
  CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
  uint32_t attempts = 1;
  apiRetStatus = CyU3PI2cReceiveBytes (preamble, data, count, 0);
  while (apiRetStatus != CY_U3P_SUCCESS && attempts <= retryCount) {
    CyU3PI2cDeInit ();
    CyFxUVCApplnI2CInit ();
    apiRetStatus = CyU3PI2cReceiveBytes (preamble, data, count, 0);
    attempts++;
  }
  return apiRetStatus;
}

/* This function inserts a delay between successful I2C transfers to prevent
   false errors due to the slave being busy.
 */
static void
AppI2CAccessDelay (
        CyU3PReturnStatus_t status)
{
    /* Add a 10us delay if the I2C operation that preceded this call was successful. */
    /*
     * if (status == CY_U3P_SUCCESS)
     *  CyU3PThreadSleep (1);
     */
}

CyU3PReturnStatus_t
I2CWriteConfirm2B (
    uint8_t slaveAddr,
    uint8_t regAddr,
    uint8_t highData,
    uint8_t lowData,
    uint32_t retryCount)
{
  CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
  uint8_t readbuf[2];
  uint32_t attempts = 0;
  bool mismatch = false;
  do {
    apiRetStatus = I2CWrite2B (slaveAddr, regAddr, highData, lowData);
    if (apiRetStatus == CY_U3P_SUCCESS) {
      mismatch = false;
      I2CRead2B (slaveAddr | ~I2C_SLAVEADDR_MASK, regAddr, readbuf);
      mismatch = mismatch || readbuf[0] != highData;
      mismatch = mismatch || readbuf[1] != lowData;
    }
    attempts++;
  } while (attempts <= retryCount && apiRetStatus == CY_U3P_SUCCESS && mismatch);
  if (apiRetStatus == CY_U3P_SUCCESS && mismatch) {
    apiRetStatus = CY_U3P_ERROR_TIMEOUT;
  }
  return apiRetStatus;
}

/* Write to an I2C slave with two bytes of data. */
CyU3PReturnStatus_t
I2CWrite2B (
        uint8_t slaveAddr,
        uint8_t regAddr,
        uint8_t highData,
        uint8_t lowData)
{
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    CyU3PI2cPreamble_t  preamble;
    uint8_t buf[2];

    /* Set the parameters for the I2C API access and then call the write API. */
    preamble.buffer[0] = slaveAddr;
    preamble.buffer[1] = regAddr;
    preamble.length    = 2;             /*  Two byte preamble. */
    preamble.ctrlMask  = 0x0000;        /*  No additional start and stop bits. */

    buf[0] = highData;
    buf[1] = lowData;

    apiRetStatus = RobustI2cTramsmitBytes (&preamble, buf, 2, NRETRY);
    AppI2CAccessDelay (apiRetStatus);

    return apiRetStatus;
}

CyU3PReturnStatus_t
I2CWriteNoReg (
        uint8_t slaveAddr,
        uint8_t data)
{
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    CyU3PI2cPreamble_t  preamble;
    uint8_t buf[1];

    /* Set the parameters for the I2C API access and then call the write API. */
    preamble.buffer[0] = slaveAddr;
    preamble.length    = 1;             /*  One byte preamble. */
    preamble.ctrlMask  = 0x0000;        /*  No additional start and stop bits. */

    buf[0] = data;

    apiRetStatus = RobustI2cTramsmitBytes (&preamble, buf, 1, NRETRY);
    AppI2CAccessDelay (apiRetStatus);

    return apiRetStatus;
}

CyU3PReturnStatus_t
I2CWrite (
        uint8_t slaveAddr,
        uint8_t regAddr,
        uint8_t count,
        uint8_t *buf)
{
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    CyU3PI2cPreamble_t preamble;

    /* Validate parameters. */
    if (count > 64)
    {
        CyU3PDebugPrint (4, "ERROR: I2CWrite count > 64\n");
        return 1;
    }

    /* Set up the I2C control parameters and invoke the write API. */
    preamble.buffer[0] = slaveAddr;
    preamble.buffer[1] = regAddr;
    preamble.length    = 2;
    preamble.ctrlMask  = 0x0000;

    apiRetStatus = RobustI2cTramsmitBytes (&preamble, buf, count, NRETRY);
    AppI2CAccessDelay (apiRetStatus);

    return apiRetStatus;
}

CyU3PReturnStatus_t
I2CSensorWrite (
          uint8_t slaveAddr,
          uint16_t regAddr,
          uint16_t regValue)
{
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    CyU3PI2cPreamble_t preamble;
    uint8_t buf[2];

       /* Set up the I2C control parameters and invoke the write API. */
    preamble.buffer[0] = slaveAddr;
    preamble.buffer[1] = regAddr >> 8;
    preamble.buffer[2] = regAddr;
    preamble.length    = 3;
    preamble.ctrlMask  = 0x0000;

    FillBuff2B(regValue, buf);
    apiRetStatus = RobustI2cTramsmitBytes (&preamble, buf, 2, NRETRY);
    AppI2CAccessDelay (apiRetStatus);

    return apiRetStatus;
}

void
I2CSensorConditionalWrite (
                CyU3PReturnStatus_t *apiRetStatus,
                uint8_t slaveAddr,
                uint16_t regAddr,
                uint16_t regValue)
{
    if (*apiRetStatus == CY_U3P_SUCCESS) {
        *apiRetStatus = I2CSensorWrite (slaveAddr, regAddr, regValue);
    }
}

CyU3PReturnStatus_t
I2CSensorRead (
           uint8_t slaveAddr,
           uint16_t regAddr,
           uint16_t *buf)
{
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    uint8_t readbuf[2];
    CyU3PI2cPreamble_t preamble;

    preamble.buffer[0] = slaveAddr & I2C_SLAVEADDR_MASK;        /*  Mask out the transfer type bit. */
    preamble.buffer[1] = regAddr >> 8;
    preamble.buffer[2] = regAddr;
    preamble.buffer[3] = slaveAddr;
    preamble.length    = 4;
    preamble.ctrlMask  = 0x0004;                                /*  Send start bit after third byte of preamble. */

    apiRetStatus = RobustI2cReceiveBytes (&preamble, readbuf, 2, NRETRY);
    *buf = Combine2B (readbuf);
    AppI2CAccessDelay (apiRetStatus);

    return apiRetStatus;
}

void
I2CSensorConditionalRead (
               CyU3PReturnStatus_t *apiRetStatus,
               uint8_t slaveAddr,
               uint16_t regAddr,
               uint16_t *buf)
{
    if (*apiRetStatus == CY_U3P_SUCCESS) {
        *apiRetStatus = I2CSensorRead(slaveAddr, regAddr, buf);
    }
}

CyU3PReturnStatus_t
I2CRead2B (
        uint8_t slaveAddr,
        uint8_t regAddr,
        uint8_t *buf)
{
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    CyU3PI2cPreamble_t preamble;

    preamble.buffer[0] = slaveAddr & I2C_SLAVEADDR_MASK;        /*  Mask out the transfer type bit. */
    preamble.buffer[1] = regAddr;
    preamble.buffer[2] = slaveAddr;
    preamble.length    = 3;
    preamble.ctrlMask  = 0x0002;                                /*  Send start bit after second byte of preamble. */

    apiRetStatus = RobustI2cReceiveBytes (&preamble, buf, 2, NRETRY);
    AppI2CAccessDelay (apiRetStatus);

    return apiRetStatus;
}

CyU3PReturnStatus_t
I2CRead (
        uint8_t slaveAddr,
        uint8_t regAddr,
        uint8_t count,
        uint8_t *buf)
{
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    CyU3PI2cPreamble_t preamble;

    /* Validate the parameters. */
    if ( count > 64 )
    {
        CyU3PDebugPrint (4, "ERROR: I2CWrite count > 64\n");
        return 1;
    }

    preamble.buffer[0] = slaveAddr & I2C_SLAVEADDR_MASK;        /*  Mask out the transfer type bit. */
    preamble.buffer[1] = regAddr;
    preamble.buffer[2] = slaveAddr;
    preamble.length    = 3;
    preamble.ctrlMask  = 0x0002;                                /*  Send start bit after second byte of preamble. */

    apiRetStatus = RobustI2cReceiveBytes (&preamble, buf, count, NRETRY);
    AppI2CAccessDelay (apiRetStatus);

    return apiRetStatus;
}

CyU3PReturnStatus_t
I2CReadNoReg (
        uint8_t slaveAddr,
        uint8_t *buf)
{
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    CyU3PI2cPreamble_t  preamble;

    /* Set the parameters for the I2C API access and then call the write API. */
    preamble.buffer[0] = slaveAddr;
    preamble.length    = 1;             /*  One byte preamble. */
    preamble.ctrlMask  = 0x0000;        /*  No additional start and stop bits. */

    apiRetStatus = RobustI2cReceiveBytes (&preamble, buf, 1, NRETRY);
    AppI2CAccessDelay (apiRetStatus);

    return apiRetStatus;
}
