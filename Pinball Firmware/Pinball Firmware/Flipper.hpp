#pragma once

#include <stdint.h>
#include "ShiftRegister.hpp"

// =============================================================================
//  Flipper.hpp
//  State machine for a single pinball flipper driven through the SPI
//  shift-register chain.
//
//  Call tick() from the Timer1 COMPA ISR (4 kHz) AFTER sr.readAll() and
//  BEFORE sr.writeAll().  The ISR rate sets the timing constants below:
//
//    4 kHz  ?  250 µs per tick
//    high_count 400 ticks  =  100 ms full-power kick
//    PWM counter 0-99      ?  40 Hz software PWM (fine for solenoid hold)
//
//  Output polarity: TPIC6C596 is open-drain.
//    setOutput(false) = gate ON  (solenoid energised)
//    setOutput(true)  = gate OFF (solenoid de-energised)
//
//  Input polarity: assumed active-HIGH from 74HC165.
//    buttonPin = 1 ? button released
//    buttonPin = 0 ? button pressed
//    eosPin    = 1 ? end-of-stroke switch triggered
// =============================================================================

struct Flipper
{
    ShiftRegister* sr;

    uint8_t outChip, outPin;   // TPIC6C596 output channel
    uint8_t eosChip, eosPin;   // 74HC165 end-of-stroke switch input
    uint8_t btnChip, btnPin;   // 74HC165 flipper button input

    uint8_t  state;            // 0=idle  1=flipping  2=holding
    uint8_t  dutyCycle;        // 0-100
    uint16_t highCount;        // kick-duration tick counter
    uint8_t  pwmCounter;       // 0-99, software PWM phase

    void init(ShiftRegister* sr_,
              uint8_t outChip_, uint8_t outPin_,
              uint8_t eosChip_, uint8_t eosPin_,
              uint8_t btnChip_, uint8_t btnPin_);

    // Call from 4 kHz Timer1 ISR after sr.readAll(), before sr.writeAll().
    void tick();
};
