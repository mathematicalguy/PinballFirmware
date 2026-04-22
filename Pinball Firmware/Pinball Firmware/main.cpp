/*
 * Pinball Firmware.cpp
 *
 * Created: 4/22/2026 3:48:39 PM
 * Author : gjp
 */ 
#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include "ShiftRegister.hpp"
#include "Flipper.hpp"

ShiftRegister sr(&PORTB, &DDRB, PB2,   // latchIn  = pin 10
				 &PORTB, &DDRB, PB1);  // latchOut = pin  9

Flipper leftFlipper;
Flipper rightFlipper;

// Timer1 COMPA ISR – fires at 4 kHz (every 250 µs)
ISR(TIMER1_COMPA_vect)
{
	sr.readAll();
	leftFlipper.tick();
	rightFlipper.tick();
	sr.writeAll();
}

int main(void)
{
	sr.begin();

	// Left  flipper: output chip 0 pin 0 | EOS chip 1 pin 3 | button chip 1 pin 2
	leftFlipper.init(&sr,  0, 0,  1, 3,  1, 2);
	// Right flipper: output chip 0 pin 1 | EOS chip 1 pin 1 | button chip 1 pin 0
	rightFlipper.init(&sr, 0, 1,  1, 1,  1, 0);

	// Timer1 CTC at 4 kHz: prescaler 8, OCR1A = (16 000 000 / 8 / 4000) - 1 = 499
	OCR1A  = 499;
	TCCR1B = (1 << WGM12) | (1 << CS11);  // CTC mode, prescaler 8
	TIMSK1 = (1 << OCIE1A);               // enable compare-match interrupt
	sei();

	while (1) {}
}
