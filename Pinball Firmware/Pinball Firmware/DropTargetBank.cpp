// =============================================================================
//  DropTargetBank.cpp
//  See DropTargetBank.hpp for wiring, timing, and usage notes.
// =============================================================================

#include "DropTargetBank.hpp"

void DropTargetBank::init(ShiftRegister* sr_,
                          uint8_t inChip_,
                          uint8_t outChip_, uint8_t outPin_,
                          volatile uint16_t* pointsAccum_)
{
    sr              = sr_;
    inChip          = inChip_;
    outChip         = outChip_;  outPin = outPin_;
    pointsAccum     = pointsAccum_;
    targetState     = 0;
    prevTargetState = 0;
    state           = 0;
    stateTimer      = 0;
}

void DropTargetBank::tick()
{
    // Read current state of the three targets (chip 2, pins 1-3)
    // Pins are LOW when target is up (not hit), HIGH when target is down (hit)
    uint8_t current = 0;
    if (!sr->readInput(inChip, 1)) current |= (1 << 0);
    if (!sr->readInput(inChip, 2)) current |= (1 << 1);
    if (!sr->readInput(inChip, 3)) current |= (1 << 2);

    if (state == 0) {
        // WATCHING – award points only on LOW->HIGH rising edges (target newly hit)
        uint8_t newlyHit = current & ~prevTargetState;

        if (newlyHit & (1 << 0)) *pointsAccum += 10;
        if (newlyHit & (1 << 1)) *pointsAccum += 10;
        if (newlyHit & (1 << 2)) *pointsAccum += 10;

        // Detect the moment all 3 go high for the first time this cycle
        bool wasAllHit = (prevTargetState == 0x07);
        bool isAllHit  = (current         == 0x07);

        if (isAllHit && !wasAllHit) {
            // All targets just hit – award bonus and start wait timer
            *pointsAccum += 50;
            state      = 1;
            stateTimer = WAIT_DURATION;
        }

    } else if (state == 1) {
        // WAITING – 3 seconds before reset
        if (--stateTimer == 0) {
            state      = 2;
            stateTimer = RESET_DURATION;
        }

    } else if (state == 2) {
        // RESETTING – pulse solenoid for up to 1 second then return to idle
        if (--stateTimer == 0) {
            sr->setOutput(outChip, outPin, true); // solenoid on
            // Stamp whatever the inputs look like right now as already-seen so
            // targets that failed to reset don't re-award points, but any that
            // did go low will score again on the next rising edge.
            prevTargetState = current;
            targetState     = current;
            state           = 0;
        } else {
            sr->setOutput(outChip, outPin, false);  // solenoid off
        }
        return; // skip prevTargetState update while resetting
    }

    prevTargetState = current;
}
