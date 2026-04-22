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
| SPI | ? In use | Shift register driver |
| Timers (T0/T1/T2) | ?? Free | |
| UART | ?? Free | |
| I2C (TWI) | ?? Free | |
| ADC | ?? Free | |
| External Interrupts (INT0/INT1) | ?? Free | |
| Pin Change Interrupts | ?? Free | |
