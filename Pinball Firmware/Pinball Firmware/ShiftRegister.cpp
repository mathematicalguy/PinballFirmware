// =============================================================================
//  ShiftRegister.cpp
//  ATmega328P  –  SPI Shift Register Driver
//
//  OUTPUT: 3x TPIC6C596DR  (Serial-In / Parallel-Out, open-drain)
//  INPUT:  3x SN74HC165DR  (Parallel-In / Serial-Out)
//
//  See ShiftRegister.hpp for full wiring and usage notes.
// =============================================================================

#include "ShiftRegister.hpp"

// SPI bus configuration for both ICs
//   Max clock: 74HC165 @ 3.3 V ≈ 10 MHz, @ 5 V ≈ 25 MHz
//              TPIC6C596 ≈ 20 MHz @ 5 V
//   4 MHz is safe for breadboard / flying-lead setups.
//   Increase to SPI_CLOCK_DIV2 (8 MHz) on a clean PCB if needed.
static const SPISettings SR_SPI_SETTINGS(4000000UL, MSBFIRST, SPI_MODE0);

// =============================================================================
//  Constructor
// =============================================================================
ShiftRegister::ShiftRegister(uint8_t latchInPin, uint8_t latchOutPin)
    : _latchInPin(latchInPin),
      _latchOutPin(latchOutPin)
{
    memset(_outputBuf, 0x00, sizeof(_outputBuf));
    memset(_inputBuf,  0x00, sizeof(_inputBuf));
}

// =============================================================================
//  begin()  –  call once in setup()
// =============================================================================
void ShiftRegister::begin()
{
    // Latch pins as outputs
    pinMode(_latchInPin,  OUTPUT);
    pinMode(_latchOutPin, OUTPUT);

    // SH/LD HIGH = shift mode (normal running state for 74HC165)
    digitalWrite(_latchInPin,  HIGH);

    // RCK LOW = hold latched outputs stable (TPIC6C596)
    digitalWrite(_latchOutPin, LOW);

    SPI.begin();

    // Push all-zero state to the output chain on startup
    writeAll();
}

// =============================================================================
//  OUTPUT  –  shadow buffer manipulation
// =============================================================================

void ShiftRegister::setOutput(uint8_t chip, uint8_t pin, bool value)
{
    if (chip >= SR_OUTPUT_CHIPS || pin > 7) return;

    if (value)
        _outputBuf[chip] |=  (1u << pin);
    else
        _outputBuf[chip] &= ~(1u << pin);
}

void ShiftRegister::setOutputByte(uint8_t chip, uint8_t data)
{
    if (chip >= SR_OUTPUT_CHIPS) return;
    _outputBuf[chip] = data;
}

bool ShiftRegister::getOutput(uint8_t chip, uint8_t pin) const
{
    if (chip >= SR_OUTPUT_CHIPS || pin > 7) return false;
    return (_outputBuf[chip] >> pin) & 0x01;
}

uint8_t ShiftRegister::getOutputByte(uint8_t chip) const
{
    if (chip >= SR_OUTPUT_CHIPS) return 0;
    return _outputBuf[chip];
}

// =============================================================================
//  writeAll()
//  Shift shadow buffer to the TPIC6C596 chain, then pulse RCK to latch.
//
//  Chain layout:  MOSI → chip[0] SER_IN … chip[N-1] SER_OUT → (end)
//
//  After shifting N bytes the LAST byte shifted sits in chip[0].
//  Therefore we transmit chip[N-1] first down to chip[0] last so that
//  _outputBuf[0] controls the chip closest to the master.
// =============================================================================
void ShiftRegister::writeAll()
{
    SPI.beginTransaction(SR_SPI_SETTINGS);

    // RCK low – keep outputs frozen while we shift new data in
    digitalWrite(_latchOutPin, LOW);

    // Shift bytes: furthest chip first so chip[0] is last loaded
    for (int8_t i = SR_OUTPUT_CHIPS - 1; i >= 0; i--)
    {
        SPI.transfer(_outputBuf[i]);
    }

    // Rising edge on RCK latches the shift register to the output drivers
    digitalWrite(_latchOutPin, HIGH);
    delayMicroseconds(1);   // t_su latch ≥ 20 ns; 1 µs is more than enough
    digitalWrite(_latchOutPin, LOW);

    SPI.endTransaction();
}

// Convenience wrappers

void ShiftRegister::writeOutput(uint8_t chip, uint8_t pin, bool value)
{
    setOutput(chip, pin, value);
    writeAll();
}

void ShiftRegister::writeOutputByte(uint8_t chip, uint8_t data)
{
    setOutputByte(chip, data);
    writeAll();
}

// =============================================================================
//  readAll()
//  Capture 74HC165 parallel inputs and store them in _inputBuf[].
//
//  Chain layout:  (master) MISO ← chip[N-1] ← … ← chip[0]
//  (Each chip's QH feeds into the SER input of the next closer to master.)
//
//  Capture sequence:
//    1. Pulse SH/LD LOW → chips load their parallel inputs simultaneously.
//    2. SH/LD HIGH → chips enter shift mode.
//    3. Clock N bytes out via SPI.transfer(0x00) – MISO only, MOSI ignored.
//
//  Byte arrival order: chip[N-1] arrives in the first SPI byte,
//  chip[0] arrives in the last SPI byte.
//  We reorder into _inputBuf[] so that index 0 always means chip 0.
//
//  Bit order inside each byte: QH (the pin that feeds the next chip / MISO)
//  is the MSB of the transfer.  For the SN74HC165 that means:
//    bit 7 = H input (parallel pin H)
//    bit 0 = A input (parallel pin A)
//  If your board wires the IC's A pin to the "first" position and you want
//  pin 0 = A, no further mapping is needed (A → bit 0 after reversing? No —
//  actually QH=H is shifted first, so received MSB = H, LSB = A).
//  Adjust the bit-reversal macro below if your PCB wires H as "pin 0".
// =============================================================================

// Optional: reverse bits in a byte so that parallel pin A appears as bit 0.
// The 74HC165 shifts H first, so without reversal bit7 = A, bit0 = H.
// Set SR_REVERSE_INPUT_BITS to 1 if you want pin index = A..H = 0..7.
#define SR_REVERSE_INPUT_BITS 0

#if SR_REVERSE_INPUT_BITS
static uint8_t reverseByte(uint8_t b)
{
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}
#endif

void ShiftRegister::readAll()
{
    SPI.beginTransaction(SR_SPI_SETTINGS);

    // --- Step 1: Load parallel inputs into all 74HC165 chips ---
    // SH/LD pulse LOW (minimum 20 ns; 1 µs is safe)
    digitalWrite(_latchInPin, LOW);
    delayMicroseconds(1);
    digitalWrite(_latchInPin, HIGH);
    // Chips are now in shift mode with inputs captured

    // --- Step 2: Clock out all bits ---
    // SPI.transfer sends 0x00 on MOSI (unused) and captures MISO.
    // chip[N-1] data arrives first (it is closest to MISO).
    for (int8_t i = SR_INPUT_CHIPS - 1; i >= 0; i--)
    {
        uint8_t raw = SPI.transfer(0x00);

#if SR_REVERSE_INPUT_BITS
        _inputBuf[i] = reverseByte(raw);
#else
        _inputBuf[i] = raw;
#endif
    }

    SPI.endTransaction();
}

// =============================================================================
//  INPUT  –  buffer read-back
// =============================================================================

bool ShiftRegister::readInput(uint8_t chip, uint8_t pin) const
{
    if (chip >= SR_INPUT_CHIPS || pin > 7) return false;
    return (_inputBuf[chip] >> pin) & 0x01;
}

uint8_t ShiftRegister::readInputByte(uint8_t chip) const
{
    if (chip >= SR_INPUT_CHIPS) return 0;
    return _inputBuf[chip];
}
