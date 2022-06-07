/*
 * serdes.h
 *
 *  Created on: Sep 14, 2017
 *      Author: Galen
 */

#ifndef SERDES_H_
#define SERDES_H_

/* I2C address for the Deserializer. */
#define DESER_ADDR_WR 0xC0
#define DESER_ADDR_RD   0xC1

/* I2C address for the Serializer. */
#define SER_ADDR_WR   0xB0
#define SER_ADDR_RD   0xB1

/* Function     : SensorConfigureSerdes
   Description  : Configure the Serdes channel to CMOS board.
   Parameters   : None
 */
extern void
SensorConfigureSerdes (
        void);

#endif /* SERDES_H_ */
