/*
 * Pinball Firmware.cpp
 *
 * Created: 4/22/2026 3:48:39 PM
 * Author : gjp
 */ 
#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "ShiftRegister.hpp"
#include "Flipper.hpp"
#include "LaunchSolenoid.hpp"
#include "DropTargetBank.hpp"
#include "RS485_USART.h"

ShiftRegister sr(&PORTB, &DDRB, PB2,   // latchIn  = pin 10
				 &PORTB, &DDRB, PB1);  // latchOut = pin  9

Flipper leftFlipper;
Flipper rightFlipper;
LaunchSolenoid launchSolenoid;
DropTargetBank dropBank;

//----------------------------------------------------------------------------
// Global Store
// --------------------------------------------------------------------------
#define SCORE_MAX 99999UL

volatile uint32_t score          = 10;
volatile uint16_t dropBankPoints = 0;   // set in ISR, drained in main loop

// ---------------------------------------------------------------------------
// Flash-window state  (input chip 1 pin 6) trigger | input chip 0 pin 4  score
// ---------------------------------------------------------------------------
static const uint16_t FLASH_DURATION  = 20000u; // 5 s  @ 4 kHz
static const uint16_t FLASH_HALF_PER  =  1000u; // 250 ms half-period (2 Hz)

volatile uint16_t flashTimer    = 0;     // ticks remaining in window; 0 = inactive
volatile uint16_t flashTick     = 0;     // free-running tick within current window
volatile bool     scoreAwarded  = false; // prevents multiple awards per window
volatile bool     addScoreFlag  = false; // set in ISR, consumed in main loop
volatile bool     rampLaneFlag  = false; // set in ISR, consumed in main loop

// ---------------------------------------------------------------------------
// Negative-mode state
//   Sequence: Top Left Lane (In0.3) -> Ball Entry Lane (In0.0) -> Right Lane (In1.6)
//   Any non-exempt rising edge between steps resets progress.
//   Exempt (never break sequence): In1 pins 0-4 (flipper/EOS/launch).
//   Completing the sequence toggles negativeMode; same sequence undoes it.
// ---------------------------------------------------------------------------
volatile bool    negativeMode = false;
volatile bool    seqDoneFlag  = false; // set in ISR, consumed in main loop
static   uint8_t seqStep     = 0;     // 0=idle  1=top-left seen  2=ball-entry seen

// Byte-wide previous-state snapshots for non-exempt inputs
static uint8_t prevSeq0 = 0;  // In0 bits 0-4
static uint8_t prevSeq1 = 0;  // In1 bits 6-7
static uint8_t prevSeq2 = 0;  // In2 bits 0-3

// Timer1 COMPA ISR – fires at 4 kHz (every 250 µs)
ISR(TIMER1_COMPA_vect)
{
	sr.readAll();
	leftFlipper.tick();
	rightFlipper.tick();
	launchSolenoid.tick();
	dropBank.tick();

	// ---- Byte-wide rising-edge detection ----
	// In1 pins 0-4 (right flipper btn, right EOS, left flipper btn, left EOS,
	// launch btn) are the ONLY inputs that never break the sequence and are
	// therefore excluded from all masks below.  Every other active input pin
	// is monitored; an unexpected rising edge while seqStep > 0 resets progress.
	const uint8_t mask0 = 0x1Fu;  // In0 pins 0-4  (Ball Entry, F, Y, Top Left, Quick Shot)
	const uint8_t mask1 = 0xC0u;  // In1 pins 6-7  (Right Lane, Left Lane) – pins 0-4 intentionally excluded
	const uint8_t mask2 = 0x0Fu;  // In2 pins 0-3  (Ramp, Drop target 1-3)

	uint8_t cur0 = sr.readInputByte(0) & mask0;
	uint8_t cur1 = sr.readInputByte(1) & mask1;
	uint8_t cur2 = sr.readInputByte(2) & mask2;

	uint8_t rise0 = cur0 & ~prevSeq0;  // In0 rising edges
	uint8_t rise1 = cur1 & ~prevSeq1;  // In1 rising edges
	uint8_t rise2 = cur2 & ~prevSeq2;  // In2 rising edges

	prevSeq0 = cur0;
	prevSeq1 = cur1;
	prevSeq2 = cur2;

	// ---- Sequence: Top Left (In0.3) ? Ball Entry (In0.0) ? Right Lane (In1.6) ----
	bool gotTopLeft   = (rise0 & (1u << 3)) != 0;
	bool gotBallEntry = (rise0 & (1u << 0)) != 0;
	bool gotRightLane = (rise1 & (1u << 6)) != 0;

	// Any unexpected non-exempt rising edge resets progress
	if (seqStep > 0) {
		uint8_t int0 = rise0;
		uint8_t int1 = rise1;
		uint8_t int2 = rise2;
		if (seqStep == 1) int0 &= ~(1u << 0);  // ball-entry is the expected next step
		if (seqStep == 2) int1 &= ~(1u << 6);  // right-lane is the expected next step
		if (int0 | int1 | int2) seqStep = 0;   // interrupted – restart
	}
	switch (seqStep) {
		case 0: if (gotTopLeft)   seqStep = 1; break;
		case 1: if (gotBallEntry) seqStep = 2; break;
		case 2:
			if (gotRightLane) {
				seqStep    = 0;
				seqDoneFlag = true;
			}
			break;
	}

	// ---- Trigger button: In1 pin 6 / Right Lane (rising-edge detect) ----
	if (gotRightLane) {
		flashTimer   = FLASH_DURATION;
		flashTick    = 0;
		scoreAwarded = false;
	}

	// ---- Flash window ----
	if (flashTimer > 0) {
		// Flash LED at 2 Hz: open-drain, false = ON, true = OFF
		bool ledOn = (flashTick % (FLASH_HALF_PER * 2)) < FLASH_HALF_PER;
		sr.setOutput(0, 7, !ledOn);
		flashTick++;
		flashTimer--;

		// Score button: In0 pin 4 (rising-edge, from rise0)
		if ((rise0 & (1u << 4)) && !scoreAwarded) {
			scoreAwarded = true;
			addScoreFlag = true;  // handled safely in main loop
		}
	} else {
		// Window expired – LED off
		sr.setOutput(0, 7, false);
	}

	// ---- Ramp lane: In2 pin 0 (rising-edge, from rise2) ----
	if (rise2 & (1u << 0)) {
		rampLaneFlag = true;
	}

	sr.writeAll();
}

// USART TX-complete ISR is defined in RS485_USART.cpp

int main(void)
{
	sr.begin();
	usart.begin(true);  // configure as master/transmitter (also calls sei())
	_delay_ms(100);     // wait for scoreboard USART to initialise after power-on
	usart.sendScore(score);
	score = score - score;
	_delay_ms(1000);
	usart.sendScore(score);

	// Mirror all input SPI port 0 bits to output SPI port 2
	sr.readAll();
	sr.setOutputByte(2, 0xAF);
	sr.writeAll();
	
	//------------------------------------------------------------------------
	// Initialization
	//------------------------------------------------------------------------
	
	// Left  flipper: output chip 0 pin 0 | EOS chip 1 pin 3 | buton chip 1 pin 2
	leftFlipper.init(&sr,  0, 0,  1, 3,  1, 2);
	// Right flipper: output chip 0 pin 1 | EOS chip 1 pin 1 | button chip 1 pin 0
	rightFlipper.init(&sr, 0, 1,  1, 1,  1, 0);
	// Launch solenoid: output chip 0 pin 2 | button chip 1 pin 4
	launchSolenoid.init(&sr, 0, 2,  1, 4);
	// Drop target bank: inputs chip 2 pins 1-3 | reset solenoid chip 0 pin 4
	dropBank.init(&sr, 2,  0, 4,  &dropBankPoints);


	//------------------------------------------------------------------------
	// History 
	//------------------------------------------------------------------------
	
	// Pre-fill debounce history with real hardware state so the edge detectors
	// in the ISR don't see a false rising edge on the very first tick.
	for (uint8_t i = 0; i < 8; i++) sr.readAll();
	prevSeq0 = sr.readInputByte(0) & 0x1Fu;  // In0 pins 0-4
	prevSeq1 = sr.readInputByte(1) & 0xC0u;  // In1 pins 6-7
	prevSeq2 = sr.readInputByte(2) & 0x0Fu;  // In2 pins 0-3

	// Pre-fill drop bank edge-detector so targets already down at power-on
	// are not treated as new hits on the first ISR tick.
	uint8_t initTargets = 0;
	if (sr.readInput(2, 1)) initTargets |= (1 << 0);
	if (sr.readInput(2, 2)) initTargets |= (1 << 1);
	if (sr.readInput(2, 3)) initTargets |= (1 << 2);
	dropBank.prevTargetState = initTargets;


	// Timer1 CTC at 4 kHz: prescaler 8, OCR1A = (16 000 000 / 8 / 4000) - 1 = 499
	OCR1A  = 499;
	TCCR1B = (1 << WGM12) | (1 << CS11);  // CTC mode, prescaler 8
	TIMSK1 = (1 << OCIE1A);               // enable compare-match interrupt
	sei();

	while (1) {
		// Negative mode: subtract points; rolling under 0 wraps to SCORE_MAX
		// Positive mode: add points; cap at SCORE_MAX
		#define APPLY_SCORE(pts) \
			do { \
				if (negativeMode) { \
					if (score >= (pts)) score -= (pts); else score = 1000UL; \
				} else { \
					score += (pts); \
					if (score > SCORE_MAX) score = SCORE_MAX; \
				} \
			} while (0)

		if (addScoreFlag) {
			addScoreFlag = false;
			APPLY_SCORE(100UL);
			usart.sendScore(score);
		}
		if (dropBankPoints > 0) {
			cli();
			uint16_t pts  = dropBankPoints;
			dropBankPoints = 0;
			sei();
			APPLY_SCORE((uint32_t)pts);
			usart.sendScore(score);
		}
		if (rampLaneFlag) {
			rampLaneFlag = false;
			APPLY_SCORE(10UL);
			usart.sendScore(score);
		}
		if (seqDoneFlag) {
			seqDoneFlag  = false;
			negativeMode = !negativeMode;
		}
	}
}
