// =============================================================================
//  Flipper.cpp
//  See Flipper.hpp for wiring, timing, and usage notes.
// =============================================================================

#include "Flipper.hpp"

void Flipper::init(ShiftRegister* sr_,
                   uint8_t outChip_, uint8_t outPin_,
                   uint8_t eosChip_, uint8_t eosPin_,
                   uint8_t btnChip_, uint8_t btnPin_)
{
    sr         = sr_;
    outChip    = outChip_;  outPin  = outPin_;
    eosChip    = eosChip_;  eosPin  = eosPin_;
    btnChip    = btnChip_;  btnPin  = btnPin_;
    state      = 0;
    dutyCycle  = 0;
    highCount  = 0;
    pwmCounter = 0;
}

void Flipper::tick()
{
    bool buttonReleased = sr->readInput(btnChip, btnPin); // button HIGH = pressed, so invert for "released"
    bool eosActive      = !sr->readInput(eosChip, eosPin); // EOS LOW = triggered, so invert for "active"

    if (buttonReleased) {
        // Button released – cut power immediately
        state      = 0;
        dutyCycle  = 0;
        pwmCounter = 0;  // reset cycle so output goes off this tick

    } else if (state == 0) {
        // New flip – full power kick
        state     = 1;
        dutyCycle = 100;
        highCount = 0;

    } else if (state == 1) {
        // Flipping – count until kick duration expires then drop to hold power
        if (highCount <= 400) {
            highCount++;
        } else {
            state     = 2;
            dutyCycle = 5;
        }

    } else if (state == 2) {
        // Holding – if EOS switch opens (ball left), kick again
        if (!eosActive) {
            state     = 1;
            dutyCycle = 100;
            highCount = 0;
        }

    } else {
        // Default / safety
        state      = 0;
        dutyCycle  = 0;
        pwmCounter = 0;
    }

    // Apply software PWM.
    // setOutput(true) = bit 1 = TPIC gate ON = drain sinks = output HIGH (coil energised)
    // setOutput(false) = bit 0 = TPIC gate OFF = drain high-Z = output LOW (coil off)
    sr->setOutput(outChip, outPin, pwmCounter >= dutyCycle);
    if (++pwmCounter >= 100) pwmCounter = 0;
}
