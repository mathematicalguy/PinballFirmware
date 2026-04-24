#pragma once

#include <stdint.h>
#include "ShiftRegister.hpp"

// =============================================================================
//  DropTargetBank.hpp
//
//  Manages a bank of 3 drop targets on input chip 2 pins 1-3.
//  Inputs go HIGH when a target is hit and stay HIGH until the bank is reset.
//
//  Scoring (written to *pointsAccum, consumed by main loop):
//    10 pts per target hit
//    50 pt bonus when all 3 are down
//
//  After all 3 targets are down the state machine waits 3 seconds, then
//  pulses the reset solenoid HIGH for up to 1 second, then returns to idle.
//
//  Call tick() from the Timer1 COMPA ISR (4 kHz) AFTER sr.readAll() and
//  BEFORE sr.writeAll().
//
//    4 kHz -> 250 us per tick
//    WAIT_DURATION  12000 ticks = 3 s
//    RESET_DURATION  4000 ticks = 1 s
// =============================================================================

struct DropTargetBank
{
    ShiftRegister* sr;

    uint8_t inChip;            // 74HC165 chip holding the three target inputs
    uint8_t outChip, outPin;   // TPIC6C596 reset solenoid output

    uint8_t  targetState;      // bitmask: bit0=pin1, bit1=pin2, bit2=pin3
    uint8_t  prevTargetState;  // previous state for rising-edge detection

    uint8_t  state;            // 0=watching  1=waiting  2=resetting
    uint16_t stateTimer;       // ticks remaining in current timed state

    volatile uint16_t* pointsAccum; // pointer to main-loop points accumulator

    static const uint16_t WAIT_DURATION  = 12000u; // 3 s @ 4 kHz
    static const uint16_t RESET_DURATION =  4000u; // 1 s @ 4 kHz

    void init(ShiftRegister* sr_,
              uint8_t inChip_,
              uint8_t outChip_, uint8_t outPin_,
              volatile uint16_t* pointsAccum_);

    // Call from 4 kHz Timer1 ISR after sr.readAll(), before sr.writeAll().
    void tick();
};
