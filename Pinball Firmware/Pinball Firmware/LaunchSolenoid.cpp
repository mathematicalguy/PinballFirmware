// =============================================================================
//  LaunchSolenoid.cpp
//  See LaunchSolenoid.hpp for wiring, timing, and usage notes.
// =============================================================================

#include "LaunchSolenoid.hpp"

void LaunchSolenoid::init(ShiftRegister* sr_,
                          uint8_t outChip_, uint8_t outPin_,
                          uint8_t btnChip_, uint8_t btnPin_)
{
    sr       = sr_;
    outChip  = outChip_;  outPin  = outPin_;
    btnChip  = btnChip_;  btnPin  = btnPin_;
    fireTimer = 0;
    prevBtn   = false;
}

void LaunchSolenoid::tick()
{
    bool btn = sr->readInput(btnChip, btnPin); // HIGH = button pressed

    // Rising edge on button triggers a 1-second pulse, ignored if already firing
    if (btn && !prevBtn && fireTimer == 0) {
        fireTimer = FIRE_DURATION;
    }
    prevBtn = btn;

    if (fireTimer > 0) {
        sr->setOutput(outChip, outPin, true); // energise solenoid
        fireTimer--;
    } else {
        sr->setOutput(outChip, outPin, false); // de-energise solenoid
    }
}
