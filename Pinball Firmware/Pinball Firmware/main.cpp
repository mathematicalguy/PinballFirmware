/*
 * Pinball Firmware.cpp
 *
 * Created: 4/22/2026 3:48:39 PM
 * Author : GJP
 */ 

#include <avr/io.h>
#include "ShiftRegister.hpp"

static const uint8_t LATCH_IN_PIN  = 10;
static const uint8_t LATCH_OUT_PIN =  9;

ShiftRegister sr(LATCH_IN_PIN, LATCH_OUT_PIN);

int main(void)
{
    sr.begin();
	sr.setOutput(0,0,true);
	sr.setOutput(0,1,true);
	sr.setOutput(2,0,true);
	sr.writeAll();    // push all three to hardware in one SPI burst
    while (1) 
    {
    }
}

