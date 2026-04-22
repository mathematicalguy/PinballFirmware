#pragma once

#include <avr/io.h>
#include <stdint.h>

// =============================================================================
//  ShiftRegister.hpp
//  ATmega328P  –  SPI Shift Register Driver  (bare-metal AVR, no Arduino)
//
//  OUTPUT chain  (3x TPIC6C596DR  –  Serial-In / Parallel-Out, open-drain)
//  INPUT  chain  (3x SN74HC165DR  –  Parallel-In / Serial-Out)
//
//  Hardware SPI pins (ATmega328P – fixed, do not use for other purposes):
//    MOSI  –  PB3  (Arduino pin 11)
//    MISO  –  PB4  (Arduino pin 12)
//    SCK   –  PB5  (Arduino pin 13)
//    SS    –  PB2  (Arduino pin 10) – must stay OUTPUT in master mode;
//                                     conveniently doubles as LATCH_IN below.
//
//  Latch pins (user-defined, passed via port/DDR/bit in constructor):
//    LATCH_IN  → SH/LD on 74HC165,   active LOW  → recommended: PB2 (pin 10)
//    LATCH_OUT → RCK   on TPIC6C596, rising edge  → recommended: PB1 (pin  9)
//
//  Typical constructor call:
//    ShiftRegister sr(&PORTB, &DDRB, PB2,   // latchIn  = pin 10
//                     &PORTB, &DDRB, PB1);  // latchOut = pin  9
//
//  IMPORTANT: F_CPU must be defined (e.g. in project settings or a top-level
//  header) before including this file so that _delay_us() is calibrated.
//  Example: #define F_CPU 16000000UL
//
//  Daisy-chain order (both chains, index 0 = closest to master):
//    Output:  MOSI → chip[0] → chip[1] → chip[2]
//    Input:   chip[0] → chip[1] → chip[2] → MISO
// =============================================================================

// Number of ICs in each chain – adjust if you add / remove chips
static const uint8_t SR_OUTPUT_CHIPS = 3;   // TPIC6C596
static const uint8_t SR_INPUT_CHIPS  = 3;   // SN74HC165

class ShiftRegister
{
	public:
	// -------------------------------------------------------------------------
	//  Constructor
	//
	//  Pass the AVR port register, DDR register, and bit position for each
	//  latch pin.  Both pointers must remain valid for the lifetime of the
	//  object (they point to hardware registers, so they always are).
	//
	//  latchIn*  – SH/LD pin on the first 74HC165  (active LOW)
	//  latchOut* – RCK   pin on the first TPIC6C596 (rising-edge latch)
	// -------------------------------------------------------------------------
	ShiftRegister(volatile uint8_t* latchInPort,  volatile uint8_t* latchInDDR,  uint8_t latchInBit,
	volatile uint8_t* latchOutPort, volatile uint8_t* latchOutDDR, uint8_t latchOutBit);

	// Call once in your init / main() before using any other method.
	// Configures latch GPIO and the SPI peripheral.
	void begin();

	// =========================================================================
	//  OUTPUT interface  (TPIC6C596 – open-drain parallel outputs)
	// =========================================================================

	// Set a single output bit in the shadow buffer.
	//   chip  : 0 … SR_OUTPUT_CHIPS-1
	//   pin   : 0 … 7  (0 = Q0 = LSB of the IC's parallel port)
	//   value : true  = gate OFF  (open-drain high-Z)
	//           false = gate ON   (drain sinks current)
	// Call writeAll() afterwards to push the buffer to hardware.
	void setOutput    (uint8_t chip, uint8_t pin, bool value);
	void setOutputByte(uint8_t chip, uint8_t data);

	// Read back what is currently in the shadow buffer (not live hardware).
	bool    getOutput    (uint8_t chip, uint8_t pin)  const;
	uint8_t getOutputByte(uint8_t chip)               const;

	// Shift the entire shadow buffer to hardware and pulse RCK to latch.
	void writeAll();

	// Convenience: update one pin/byte and immediately call writeAll().
	void writeOutput    (uint8_t chip, uint8_t pin, bool value);
	void writeOutputByte(uint8_t chip, uint8_t data);

	// =========================================================================
	//  INPUT interface  (SN74HC165 – parallel-in / serial-out)
	// =========================================================================

	// Capture all parallel inputs from the 74HC165 chain into the internal
	// buffer.  Must be called before readInput() / readInputByte().
	void readAll();

	// Read a single captured bit.
	//   chip : 0 … SR_INPUT_CHIPS-1
	//   pin  : 0 … 7  (pin 0 = parallel input A of that IC)
	bool    readInput    (uint8_t chip, uint8_t pin)  const;
	uint8_t readInputByte(uint8_t chip)               const;

	private:
	// --- latch pin descriptors ---
	volatile uint8_t* _latchInPort;
	volatile uint8_t* _latchInDDR;
	uint8_t           _latchInBit;

	volatile uint8_t* _latchOutPort;
	volatile uint8_t* _latchOutDDR;
	uint8_t           _latchOutBit;

	// --- shadow buffers ---
	uint8_t _outputBuf[SR_OUTPUT_CHIPS];
	uint8_t _inputBuf [SR_INPUT_CHIPS];

	// --- private helpers ---
	inline void latchInHigh()  { *_latchInPort  |=  (1u << _latchInBit);  }
	inline void latchInLow()   { *_latchInPort  &= ~(1u << _latchInBit);  }
	inline void latchOutHigh() { *_latchOutPort |=  (1u << _latchOutBit); }
	inline void latchOutLow()  { *_latchOutPort &= ~(1u << _latchOutBit); }

	static uint8_t spiTransfer(uint8_t data);
};