/*
 * serdes.c
 *
 *
 *  Created on: Sep 14, 2017
 *      Author: Galen
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
#include "serdes.h"
#include "sensor.h"
#include "auxiliary.h"
#include "appi2c.h"

/* Deserializer registers */
#define DESER_GENERAL_CFG1 0x03
#define DESER_LF_GAIN      0x05
#define SER_ALIAS          0x07
#define SLAVE_ID_0         0x08
#define SLAVE_ID_1         0x09
#define SLAVE_ALIAS_0      0x10
#define SLAVE_ALIAS_1      0x11
#define DESER_GPIO_CONFIG  0x1D
#define DESER_BCC_WDT      0x20

/* Serializer registers */
#define SER_PWR_RESET     0x01
#define SER_GENERAL_CFG   0x03
#define SER_GPO_CONFIG    0x0D
#define SER_SCL_HIGH      0x11
#define SER_SCL_LOW       0x12
#define SER_BCC_WDT       0x1E
#define SER_PLL_OVERWRITE 0x35

//sets the Bidirectional Control Channel
//Watchdog Timeout value in units of 2ms. This
//field should not be set to 0
#define BCC_WDT_TIMEOUT    0x02
#define SER_SCL_TIME_VAL   0x32

void
SensorConfigureSerdes (
    void)
{
  uint8_t buf[1];

  /* Boost low frequency gain */

//  buf[0] = 0x00;
  buf[0] = 0xC0;
  I2CWrite (DESER_ADDR_WR, DESER_LF_GAIN, 1, buf);


  // Configure watchdog timer for deserializer
  buf[0] = BCC_WDT_TIMEOUT;
  I2CWrite (DESER_ADDR_WR, DESER_BCC_WDT, 1, buf);

  // Configure I2C passthrough for Serializer
  buf[0] = SER_ADDR_WR;
  I2CWrite (DESER_ADDR_WR, SER_ALIAS, 1, buf); // Write to SER alias register

  /* Latch pixel data into serializer on falling pclk */
  buf[0] = 0xC5 & ~0x01; // default is 0xC5, set TRFB (0th bit) to zero
  I2CWrite (SER_ADDR_WR, SER_GENERAL_CFG, 1, buf);

  // Configure watchdog timer for serializer
  buf[0] = BCC_WDT_TIMEOUT;
  I2CWrite (SER_ADDR_WR, SER_BCC_WDT, 1, buf);

  // Configure I2C master scl period
  buf[0] = SER_SCL_TIME_VAL;
  I2CWrite (SER_ADDR_WR, SER_SCL_HIGH, 1, buf);
  I2CWrite (SER_ADDR_WR, SER_SCL_LOW,  1, buf);

  // Configure I2C passthrough for imaging sensor
  buf[0] = SENSOR_ADDR_WR;
  I2CWrite (DESER_ADDR_WR, SLAVE_ID_0, 1, buf);
  I2CWrite (DESER_ADDR_WR, SLAVE_ALIAS_0, 1, buf);

  // Configure I2C passthrough for MCU
  buf[0] = AUX_ADDR_WR;
  I2CWrite (DESER_ADDR_WR, SLAVE_ID_1, 1, buf);
  I2CWrite (DESER_ADDR_WR, SLAVE_ALIAS_1, 1, buf);
}
