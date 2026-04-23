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
#include "RS485_USART.h"

ShiftRegister sr(&PORTB, &DDRB, PB2,   // latchIn  = pin 10
				 &PORTB, &DDRB, PB1);  // latchOut = pin  9

Flipper leftFlipper;
Flipper rightFlipper;
LaunchSolenoid launchSolenoid;

volatile uint16_t score = 10;

// ---------------------------------------------------------------------------
// Flash-window state  (input chip 1 pin 5 ? trigger | input chip 0 pin 4 ? score)
// ---------------------------------------------------------------------------
static const uint16_t FLASH_DURATION  = 20000u; // 5 s  @ 4 kHz
static const uint16_t FLASH_HALF_PER  =  1000u; // 250 ms half-period (2 Hz)

volatile uint16_t flashTimer    = 0;     // ticks remaining in window; 0 = inactive
volatile uint16_t flashTick     = 0;     // free-running tick within current window
volatile bool     scoreAwarded  = false; // prevents multiple awards per window
volatile bool     addScoreFlag  = false; // set in ISR, consumed in main loop
static   bool     prevTrigBtn   = false; // previous state of trigger button
static   bool     prevScoreBtn  = false; // previous state of score button

// Timer1 COMPA ISR – fires at 4 kHz (every 250 µs)
ISR(TIMER1_COMPA_vect)
{
	sr.readAll();
	leftFlipper.tick();
	rightFlipper.tick();
	launchSolenoid.tick();

	// --- Trigger button: input chip 1 pin 4 (rising-edge detect) ---
	bool trigBtn = sr.readInput(1, 6);
	if (trigBtn && !prevTrigBtn) {
		// Rising edge – start (or restart) the 5-second flash window
		flashTimer   = FLASH_DURATION;
		flashTick    = 0;
		scoreAwarded = false;
	}
	prevTrigBtn = trigBtn;

	// --- Flash window ---
	if (flashTimer > 0) {
		// Flash LED at 2 Hz: open-drain, false = ON, true = OFF
		bool ledOn = (flashTick % (FLASH_HALF_PER * 2)) < FLASH_HALF_PER;
		sr.setOutput(0, 7, !ledOn);
		flashTick++;
		flashTimer--;

		// Score button: input chip 0 pin 4 (rising-edge detect)
		bool scoreBtn = sr.readInput(0, 4);
		if (scoreBtn && !prevScoreBtn && !scoreAwarded) {
			scoreAwarded = true;
			addScoreFlag = true;  // handled safely in main loop
		}
		prevScoreBtn = scoreBtn;
	} else {
		// Window expired – LED off
		sr.setOutput(0, 7, false);
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
	score = score + 2;
	_delay_ms(1000);
	usart.sendScore(score);

	// Mirror all input SPI port 0 bits to output SPI port 2
	sr.readAll();
	sr.setOutputByte(2, sr.readInputByte(0));
	sr.writeAll();

	// Left  flipper:
	leftFlipper.init(&sr,  0, 0,  1, 3,  1, 2);
	// Right flipper: output chip 0 pin 1 | EOS chip 1 pin 1 | button chip 1 pin 0
	rightFlipper.init(&sr, 0, 1,  1, 1,  1, 0);
	// Launch solenoid: output chip 0 pin 2 | button chip 1 pin 4
	launchSolenoid.init(&sr, 0, 2,  1, 4);

	// Timer1 CTC at 4 kHz: prescaler 8, OCR1A = (16 000 000 / 8 / 4000) - 1 = 499

	// Pre-fill debounce history with real hardware state so the edge detectors
	// in the ISR don't see a false rising edge on the very first tick.
	for (uint8_t i = 0; i < 8; i++) sr.readAll();
	prevTrigBtn  = sr.readInput(1, 4);
	prevScoreBtn = sr.readInput(0, 4);

	OCR1A  = 499;
	TCCR1B = (1 << WGM12) | (1 << CS11);  // CTC mode, prescaler 8
	TIMSK1 = (1 << OCIE1A);               // enable compare-match interrupt
	sei();

	while (1) {
		if (addScoreFlag) {
			addScoreFlag = false;
			score += 100;
			usart.sendScore(score);
		}
	}
}
