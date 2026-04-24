# PinballFirmware
Source Code for our Pinball Design Project Final

## SPI Pin Usage (ATmega328P)
Uses the hardware SPI peripheral — the following pins are reserved and cannot be used for anything else:

| Pin | Port | Arduino Pin | Function |
|-----|------|-------------|----------|
| MOSI | PB3 | 11 | Serial data out ? shift registers |
| MISO | PB4 | 12 | Serial data in ? shift registers |
| SCK | PB5 | 13 | Clock |
| SS / LATCH_IN | PB2 | 10 | 74HC165 SH/LD (latch inputs), must stay OUTPUT in SPI master mode |
| LATCH_OUT | PB1 | 9 | TPIC6C596 RCK (latch outputs) |

## Hardware Resources
| Peripheral | Status | Notes |
|------------|--------|-------|
| SPI |  In use | Shift register driver |
| Timer1 | In use | 4 kHz flipper ISR (CTC, prescaler 8, OCR1A=499) |
| Timer2 | In use | RS485 USART transmit cadence (~100 Hz sub-tick, 30 ms effective, CTC, prescaler 1024, OCR2A=155) |
| Timer0 |  Free | |
| UART | In use | RS485 half-duplex, 9N1, 250 kBaud, MPCM multiprocessor mode |
| I2C (TWI) | Free | |
| ADC |  Free | |
| External Interrupts (INT0/INT1) | Free | |
| Pin Change Interrupts |  Free | |

## RS485 USART (ATmega328P)
9-bit Multiprocessor Communication Mode over RS485 half-duplex transceiver.

| Parameter | Value |
|-----------|-------|
| Baud rate | 250,000 bps |
| Frame format | 9N1 (9 data bits, no parity, 1 stop bit) |
| UBRR | 3 |
| RTS/DE pin | PD2 (Arduino pin 2) — HIGH = transmit, LOW = receive |

**Protocol:** Master sends a 6-byte burst on demand via `sendScore(uint16_t score)` containing the node address, two register address/value pairs (lower and upper score bytes), and a stop packet. Receivers use MPCM to ignore traffic not addressed to their node.

| Pin | Port | Arduino Pin | Function |
|-----|------|-------------|----------|
| TX | PD1 | 1 | Serial data out to RS485 transceiver |
| RX | PD0 | 0 | Serial data in from RS485 transceiver |
| RTS/DE | PD2 | 2 | Transmit enable (HIGH) / Receive enable (LOW) |

## SPI Shift Register Pin Usage

### Output Chips (TPIC6C596 - open-drain, 3x daisy-chained)

**Out 0**

| Pin | Function                        |
|-----|---------------------------------|
|  0  | Left flipper solenoid           |
|  1  | Right flipper solenoid          |
|  2  | Launch solenoid                 |
|  3  | (unassigned)                    |
|  4  | Drop target bank reset solenoid |
|  5  | (unassigned) - LED burnt out    |
|  6  | (unassigned) - Led burnt out    |
|  7  | Quick Shot LED                  |

**Out 1**

| Pin | Function     |
|-----|--------------|
|  0  | F lane Hit   |
|  1  | Y lane Hit   |
|  2  | (unassigned) |
|  3  | (unassigned) |
|  4  | (unassigned) |
|  5  | (unassigned) |
|  6  | (unassigned) |
|  7  | (unassigned) |

**Out 2**

| Pin | Function     |
|-----|--------------|
|  0  | (unassigned) |
|  1  | (unassigned) |
|  2  | (unassigned) |
|  3  | (unassigned) |
|  4  | (unassigned) |
|  5  | (unassigned) |
|  6  | (unassigned) |
|  7  | (unassigned) |

### Input Chips (SN74HC165 - active HIGH, 3x daisy-chained)

**In 0**

| Pin | Function                  |
|-----|---------------------------|
|  0  | Ball Entry Lane           |
|  1  | F lane                    |
|  2  | Y lane                    |
|  3  | Top left Lane             |
|  4  | Quick Shot Target  hit    |
|  5  | (unassigned)              |
|  6  | (unassigned)              |
|  7  | (unassigned)              |

**In 1**

| Pin | Function                 |
|-----|-------------------------|
|  0  | Right flipper button     |
|  1  | Right flipper EOS switch |
|  2  | Left flipper button      |
|  3  | Left flipper EOS switch  |
|  4  | Launch button            |
|  5  | No connecton             |
|  6  | Right Pinball lane       |
|  7  | Left Pinball lane        |

**In 2**

| Pin | Function      |
|-----|---------------|
|  0  | Ramp lane	  |
|  1  | Drop target 1 |
|  2  | Drop target 2 |
|  3  | Drop target 3 |
|  4  | (unassigned)  |
|  5  | (unassigned)  |
|  6  | (unassigned)  |
|  7  | (unassigned)  |