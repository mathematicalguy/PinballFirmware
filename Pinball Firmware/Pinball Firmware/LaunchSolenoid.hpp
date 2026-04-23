#pragma once

#include <stdint.h>
#include "ShiftRegister.hpp"

// =============================================================================
//  LaunchSolenoid.hpp
//
//  When the launch button input goes HIGH (rising edge) the solenoid output
//  is driven HIGH for a fixed 1-second pulse, then cut.
//
//  Call tick() from the Timer1 COMPA ISR (4 kHz) AFTER sr.readAll() and
//  BEFORE sr.writeAll().
//
//    4 kHz  ->  250 us per tick
//    FIRE_DURATION 4000 ticks  =  1 s full-power pulse
// =============================================================================

struct LaunchSolenoid
{
    ShiftRegister* sr;

    uint8_t  outChip,  outPin;   // TPIC6C596 output channel
    uint8_t  btnChip,  btnPin;   // 74HC165 launch button input (active HIGH)

    uint16_t fireTimer;          // ticks remaining in fire pulse; 0 = idle
    bool     prevBtn;            // previous button state for edge detection

    static const uint16_t FIRE_DURATION = 4000u; // 1 s @ 4 kHz

    void init(ShiftRegister* sr_,
              uint8_t outChip_, uint8_t outPin_,
              uint8_t btnChip_, uint8_t btnPin_);

    // Call from 4 kHz Timer1 ISR after sr.readAll(), before sr.writeAll().
    void tick();
};
