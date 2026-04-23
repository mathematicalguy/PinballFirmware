// =============================================================================
//  ShiftRegister.cpp
//  ATmega328P  –  SPI Shift Register Driver  (bare-metal AVR, no Arduino)
//
//  OUTPUT: 3x TPIC6C596DR  (Serial-In / Parallel-Out, open-drain)
//  INPUT:  3x SN74HC165DR  (Parallel-In / Serial-Out)
//
//  See ShiftRegister.hpp for full wiring and usage notes.
// =============================================================================

#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#include "ShiftRegister.hpp"
#include <avr/io.h>
#include <util/delay.h>   // _delay_us() – requires F_CPU to be defined
#include <string.h>       // memset

// =============================================================================
//  SPI helper
//
//  ATmega328P hardware SPI registers:
//    SPCR – SPI Control Register
//    SPSR – SPI Status Register  (also holds SPI2X double-speed bit)
//    SPDR – SPI Data Register    (write to send, read after SPIF to receive)
//
//  Clock rate selection (assuming F_CPU = 16 MHz):
//    SPR1 SPR0  SPI2X   Divisor   Rate
//      0    0     0       /4      4 MHz   ← used here (safe for breadboards)
//      0    0     1       /2      8 MHz   ← fine on a clean PCB
//      0    1     0       /16     1 MHz
//  74HC165 max @ 5 V ≈ 25 MHz, TPIC6C596 max ≈ 20 MHz – 4 MHz has headroom.
// =============================================================================

// SPI bit-field helpers (keeps the code readable without <avr/io.h> magic numbers)
#define SPI_SPE   (1u << SPE)    // SPI Enable
#define SPI_MSTR  (1u << MSTR)   // Master mode
#define SPI_SPIF  (1u << SPIF)   // Transfer-complete flag

uint8_t ShiftRegister::spiTransfer(uint8_t data)
{
	SPDR = data;                       // start transmission
	while (!(SPSR & SPI_SPIF)) {}      // wait for SPIF (transfer complete)
	return SPDR;                       // reading SPDR clears SPIF
}

// =============================================================================
//  Constructor
// =============================================================================
ShiftRegister::ShiftRegister(volatile uint8_t* latchInPort,  volatile uint8_t* latchInDDR,  uint8_t latchInBit,
volatile uint8_t* latchOutPort, volatile uint8_t* latchOutDDR, uint8_t latchOutBit)
: _latchInPort (latchInPort),
_latchInDDR  (latchInDDR),
_latchInBit  (latchInBit),
_latchOutPort(latchOutPort),
_latchOutDDR (latchOutDDR),
_latchOutBit (latchOutBit)
{
	memset(_outputBuf,        0x00, sizeof(_outputBuf));
	memset(_inputBuf,         0x00, sizeof(_inputBuf));
	memset(_debouncedBuf,     0x00, sizeof(_debouncedBuf));
	memset(_debounceHistory,  0x00, sizeof(_debounceHistory));
	_debounceHead = 0;
}

// =============================================================================
//  begin()  –  call once before using any other method
// =============================================================================
void ShiftRegister::begin()
{
	// --- Configure latch GPIO as outputs ---
	*_latchInDDR  |= (1u << _latchInBit);
	*_latchOutDDR |= (1u << _latchOutBit);

	// SH/LD HIGH = shift mode (idle state for 74HC165)
	latchInHigh();

	// RCK LOW = hold TPIC6C596 outputs stable while we set up
	latchOutLow();

	// --- Configure hardware SPI ---
	// MOSI (PB3) and SCK (PB5) must be outputs; MISO (PB4) is input by default.
	// PB2 (SS) must be output to keep the peripheral in master mode;
	// it is typically the same pin as latchIn, which was already set above.
	DDRB |= (1u << PB3) | (1u << PB5) | (1u << PB2);
	DDRB &= ~(1u << PB4);   // MISO = input

	// SPCR: enable SPI, master, MSB first, MODE 0 (CPOL=0, CPHA=0), clk/4
	SPCR = SPI_SPE | SPI_MSTR;   // SPR1=0, SPR0=0 → clk/4; CPOL=0, CPHA=0
	SPSR &= ~(1u << SPI2X);      // no double speed → 4 MHz @ 16 MHz F_CPU

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
//  Therefore we transmit chip[N-1] first and chip[0] last so that
//  _outputBuf[0] controls the chip closest to the master.
// =============================================================================
void ShiftRegister::writeAll()
{
	// RCK low – keep outputs frozen while we shift new data in
	latchOutLow();

	// Shift bytes: furthest chip first so chip[0] is last loaded
	for (int8_t i = (int8_t)SR_OUTPUT_CHIPS - 1; i >= 0; i--)
	{
		spiTransfer(_outputBuf[i]);
	}

	// Rising edge on RCK latches shift-register data to the output drivers.
	// t_su latch ≥ 20 ns; 1 µs is more than enough.
	latchOutHigh();
	_delay_us(1);
	latchOutLow();
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
//    1. Pulse SH/LD LOW → chips latch their parallel inputs simultaneously.
//    2. SH/LD HIGH → enter shift mode.
//    3. Clock N bytes with spiTransfer(0x00) – MISO captured, MOSI ignored.
//
//  Byte arrival order: chip[N-1] arrives in the first SPI byte,
//  chip[0] arrives in the last.  We reorder into _inputBuf[] so that
//  index 0 always refers to chip 0.
//
//  Bit order: the 74HC165 shifts H first, so without reversal:
//    received MSB (bit 7) = parallel pin H
//    received LSB (bit 0) = parallel pin A
//  Set SR_REVERSE_INPUT_BITS to 1 below if you want bit 0 = pin A.
// =============================================================================

#define SR_REVERSE_INPUT_BITS 0

#if SR_REVERSE_INPUT_BITS
static uint8_t reverseByte(uint8_t b)
{
	b = (uint8_t)((b & 0xF0u) >> 4 | (b & 0x0Fu) << 4);
	b = (uint8_t)((b & 0xCCu) >> 2 | (b & 0x33u) << 2);
	b = (uint8_t)((b & 0xAAu) >> 1 | (b & 0x55u) << 1);
	return b;
}
#endif

void ShiftRegister::readAll()
{
	// --- Step 1: pulse SH/LD LOW to load parallel inputs into all chips ---
	// Minimum load pulse width = 20 ns; 1 µs is safe.
	latchInLow();
	_delay_us(1);
	latchInHigh();
	// Chips now hold their sampled inputs and are ready to shift

	// --- Step 2: clock out all bits ---
	// Send dummy 0x00 on MOSI (unused by 74HC165); capture MISO.
	// chip[N-1] is closest to MISO so its data arrives first.
	for (uint8_t i = 0; i < SR_INPUT_CHIPS; i++)
	{
		uint8_t raw = spiTransfer(0x00);

		#if SR_REVERSE_INPUT_BITS
		raw = reverseByte(raw);
		#endif

		_inputBuf[i]                       = raw;
		_debounceHistory[i][_debounceHead] = raw;
	}

	// Advance circular buffer head
	_debounceHead = (_debounceHead + 1) & (SR_DEBOUNCE_SAMPLES - 1);

	// Update debounced state: a bit is 1 only if all samples are 1,
	//                         a bit is 0 only if all samples are 0,
	//                         otherwise it holds its last stable value.
	for (uint8_t i = 0; i < SR_INPUT_CHIPS; i++)
	{
		uint8_t allHigh = 0xFF;
		uint8_t allLow  = 0xFF;
		for (uint8_t s = 0; s < SR_DEBOUNCE_SAMPLES; s++)
		{
			allHigh &=  _debounceHistory[i][s];  // bits that are 1 in every sample
			allLow  &= ~_debounceHistory[i][s];  // bits that are 0 in every sample
		}
		_debouncedBuf[i] = (_debouncedBuf[i] & ~(allHigh | allLow))
						 | (allHigh);
	}
}

// =============================================================================
//  INPUT  –  buffer read-back
// =============================================================================

bool ShiftRegister::readInput(uint8_t chip, uint8_t pin) const
{
	if (chip >= SR_INPUT_CHIPS || pin > 7) return false;
	return (_debouncedBuf[chip] >> pin) & 0x01;
}

uint8_t ShiftRegister::readInputByte(uint8_t chip) const
{
	if (chip >= SR_INPUT_CHIPS) return 0;
	return _debouncedBuf[chip];
}