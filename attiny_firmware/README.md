# THIS CODE CONTAINS MATERIAL PROTECTED UNDER NDA WITH ARROW, DO NOT RELEASE THIS TO THE PUBLIC!!!!

# Firmware for the ATtiny 816 MCU onboard the 'feather-scope' fee-scope, Revision 2.0

This firmware programs the onboard attiny 816 to act as a I2C to SPI bridge,
sample analog inputs, and finally transmit analog samples along with sync
information to the SERDES. This version of the firmware is intended to control
the ON-semi Python 480 image sensor, featherscope revison 2.0.

## Microcontroller's Behavior and Responsibilities

The ATTiny 816 microcontroller on the pico-scope is responsible for three things:

1. Initial configuration of the E2V image sensor
2. Conversion of I2C messages from the serializer to SPI messages to the E2V imaging sensor
3. Sampling its analog inputs and transmitting ADC reads to the serializer

Each of these functions are controlled through the MCU's I2C interface. The code
controlling the I2C interace is in `twi_slave.c`. In brief, the ATTiny 186
responds to two I2C addresses.

| Sub Device         | Description                                                          | I2C (write) address |
|--------------------|----------------------------------------------------------------------|---------------------|
| I2C -> SPI Bridge  | Convert I2C commands from serializer to SPI commands to image sensor | `B8h`               |
| ADC (self)         | Control ADC subsystem                                                | `98h`               |

### I2C -> SPI Bridge

The E2V image sensor can be accessed via the ATTiny's I2C interface. Reads and
writes to registers on the E2V are accomplished by addressing the same
'register' on the I2C bridge device. This is true for all registers except the
reserved register, `FFh`, which triggers the reset and initial configuration of
the E2V sensor.

| Register    | Description    | Effect                                                               |
|-------------|----------------|----------------------------------------------------------------------|
| `00h`-`FEh` | Pass through   | Reads and writes to the I2C device are repeated on the SPI interface |
| `FFh`       | Initialization | E2V image sensor is reset and configured according to EEPROM values  |

### ADC Subdevice

The ADC can be controlled via the 'self' device by writing to the following registers:

| Register Name     | Address | bit fields            | description                                                                 |
|-------------------|---------|-----------------------|-----------------------------------------------------------------------------|
| ADC control       | `01h`   | [0000-00me]           | e bit is enable (1 is on), m bit requests multiplexing (sample both inputs) |
| ADC sample period | `02h`   | [bbbb-bbbb-bbbb-bbbb] | Number of CPU clock cycles between ADC samples, two bytes                   |

#### ADC Behavior

The ADC is read after `N` CPU clock cycles, where `N` is set by the ADC sample
period register. It must be at least `0034h`.

The USART does not use inverted output (e.g. low is 0), and transmits the least
significant bit first.

If multiplexing is requested, the ADC will sample AIN8 and AIN9 in round robin.
Otherwise AIN8 alone is sampled.

### USART output of ADC reads

ADC reads will be transmitted over the USART interface with synchronous serial
output. ADC reads are transmitted in two bytes. The LSB of the ADC read is
transmitted in the first byte. The second byte is [ffprssaa], most significant
bit on the left, where ff is the frame indicator (`01b`), p is the adc input bit
(0 for AIN8, 1, for AIN9), r is reserved (intended for overflow), ss are the
sync variables (LV - msb, FV -lsb) from frame enable and line enable, and aa are
the two most significant bits from the ADC read, least significant bit in the
least significant position.

Each byte is transmitted in a 10 bit frame, with the first bit being the frame
start indicator (always low), and the last bit being the frame start indicator
(always high), and the eight data bits will be in between. The TXD
(transmission) and XCK (transmission clock) lines of the USART peripheral will
be used to transmit the data. The transmission clock will idle low, and the
on the TXD line will be valid on the rising edge of the clock signal. The data
will change on the falling edge of the clock signal. The TXD line idles high.The
8 data bits will be transmitted most significant bit first.

## Building
Compile the code with `release` configuration prior to programming the
microcontroller, unless you're debugging with the UPDI interface.

ADC timings are only tested with `release` code, so use `debug` at your own risk.

## Programming
Some of the startup register settings for the Python 480 image sensor are
proprietary and can only be accessed by signing a non-disclosure agreement with
an ONSemi distributor. We are legally obligated to redact portions of the source
code containing this information. The compiled code is contained in the Release
folder and can be uploaded directly to the microcontroller using Atmel Studio.

## High level overview of the code

Functionality is mostly implemented through interrupts. The I2C (called TWI by
Atmel) subsystem is exclusively driven by interrupts. ADC sampling is not done
with interrupts, but instead polls the ADC conversion register for results.
However, the ADC programming is controlled by global variables that are set by
I2C interrupts. E2V intialization makes heavy use of data EEPROM. See programming
notes for caveats about writing these EEPROM values. Many functions are
'inlined', and therefore appear in header files.

| functionality      | files                                     |
|--------------------|-------------------------------------------|
| I2C -> SPI bridge  | `twi_slave`, `spi_master`                 |
| E2V initialization | `spi_master`, `twi_slave`                 |
| ADC control        | `adc`, `usart_out`, `twi_slave`, `main`   |
