/*
 * Pinball Firmware.cpp
 *
 * Created: 4/22/2026 3:48:39 PM
 * Author : gjp
 */ 
#define F_CPU 16000000UL  // must be defined before util/delay.h

#ifndef __AVR_ATmega328P__
#define __AVR_ATmega328P__
#endif

#include <avr/io.h>
#include "ShiftRegister.hpp"

ShiftRegister sr(&PORTB, &DDRB, PB2,   // latchIn  = pin 10
&PORTB, &DDRB, PB1);  // latchOut = pin  9

int main(void)
{
	sr.begin();
	sr.setOutput(0, 0, true);
	sr.setOutput(1, 1, true);
	sr.setOutput(2, 2, true);
	sr.writeAll();
	while (1) {}
}
