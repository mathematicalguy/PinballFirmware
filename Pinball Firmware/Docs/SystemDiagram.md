# System Diagram

```mermaid
block-beta
  columns 5

  %% ── Row 1: ATmega328P ───────────────────────────────────────────────────
  MCU["ATmega328P\n(Arduino Uno R3)\nF_CPU = 16 MHz\nTimer1 ISR @ 4 kHz"]:5

  %% ── Row 2: SPI bus labels ───────────────────────────────────────────────
  space:1
  SPI_OUT_BUS["SPI Output Chain\nMOSI PB3 · SCK PB5\nLATCH_OUT RCK PB1 (pin 9)"]:2
  SPI_IN_BUS["SPI Input Chain\nMISO PB4 · SCK PB5\nLATCH_IN SH/LD PB2 (pin 10)"]:2

  %% ── Row 3: Output ICs ───────────────────────────────────────────────────
  space:1
  OUT0["TPIC6C596 #0\n(Output Chip 0)\nOpen-Drain x8"]:1
  OUT1["TPIC6C596 #1\n(Output Chip 1)\nOpen-Drain x8"]:1
  OUT2["TPIC6C596 #2\n(Output Chip 2)\nOpen-Drain x8"]:1
  space:1

  %% ── Row 4: Input ICs ────────────────────────────────────────────────────
  space:1
  IN0["SN74HC165 #0\n(Input Chip 0)\nParallel-In x8"]:1
  IN1["SN74HC165 #1\n(Input Chip 1)\nParallel-In x8"]:1
  IN2["SN74HC165 #2\n(Input Chip 2)\nParallel-In x8"]:1
  space:1

  %% ── Row 5: RS485 block ──────────────────────────────────────────────────
  RS485["RS485 Transceiver\nDE/RE → PD2 (pin 2)\nTX  → PD1 (pin 1)\n9N1 · 250 000 bps\nhalf-duplex"]:2
  space:1
  SB["Score Board\n(RS485 Receiver)\nAddr 0x10\nLower/Upper/High byte regs"]:2

  %% ── Edges ───────────────────────────────────────────────────────────────
  MCU --> SPI_OUT_BUS
  MCU --> SPI_IN_BUS
  SPI_OUT_BUS --> OUT0
  OUT0 --> OUT1
  OUT1 --> OUT2
  SPI_IN_BUS --> IN0
  IN0 --> IN1
  IN1 --> IN2
  MCU --> RS485
  RS485 --> SB
```

---

## SPI Output Chain — TPIC6C596 Pin Assignments

| Chip | Pin | Signal | Function |
|------|-----|--------|----------|
| Out 0 | 0 | Left Flipper Solenoid | Full-power kick → PWM hold |
| Out 0 | 1 | Right Flipper Solenoid | Full-power kick → PWM hold |
| Out 0 | 2 | Launch Solenoid | 1 s fire pulse on button press |
| Out 0 | 4 | Drop Target Reset Solenoid | Pulsed 1 s after 3 s wait on full clear |
| Out 0 | 7 | Hurry-Up LED | Flashes 2 Hz during 5 s hurry-up window |
| Out 1 | 0 | F Lane LED | Lit when F top-lane rollover is active |
| Out 1 | 1 | Y Lane LED | Lit when Y top-lane rollover is active |

---

## SPI Input Chain — SN74HC165 Pin Assignments

| Chip | Pin | Signal | Function |
|------|-----|--------|----------|
| In 0 | 0 | Ball Entry Lane | Step 2 of mode-sequence; debounced |
| In 0 | 1 | F Lane Rollover | Lights F LED; top-lane jackpot logic |
| In 0 | 2 | Y Lane Rollover | Lights Y LED; top-lane jackpot logic |
| In 0 | 3 | Top Left Lane | Step 1 of mode-sequence |
| In 0 | 4 | Quick Shot Button | Awards +100 pts during hurry-up window |
| In 1 | 0 | Right Flipper Button | Flipper control + lane-light rotation |
| In 1 | 1 | Right Flipper EOS | End-of-stroke cuts kick phase early |
| In 1 | 2 | Left Flipper Button | Flipper control + lane-light rotation |
| In 1 | 3 | Left Flipper EOS | End-of-stroke cuts kick phase early |
| In 1 | 4 | Launch Button | Triggers 1 s launch solenoid pulse |
| In 1 | 6 | Right Lane | Step 3 of mode-sequence; opens hurry-up window |
| In 1 | 7 | Left Lane | General lane input |
| In 2 | 0 | Ramp Exit Sensor | Awards +10 pts per ball passage |
| In 2 | 1 | Drop Target 1 | +10 pts on hit; contributes to full-clear bonus |
| In 2 | 2 | Drop Target 2 | +10 pts on hit; contributes to full-clear bonus |
| In 2 | 3 | Drop Target 3 | +10 pts on hit; +50 bonus on full clear |

---

## RS485 / Score Board Protocol

| Detail | Value |
|--------|-------|
| MCU USART | USART0, 9N1, 250 000 bps |
| Direction control | PD2 HIGH = TX, LOW = RX |
| Frame type | 9-bit Multiprocessor Communication Mode |
| Scoreboard address frame | `0x110` (bit 8 = 1, addr = 0x10) |
| Register — lower byte | `0x001` |
| Register — upper byte | `0x002` |
| Register — high byte | `0x003` |
| Stop packet | `0x1FF` (bit 8 = 1) |
| TX buffer size | 8 frames per score update |
| Score range | 0 – 99 999 |

---

## Control Flow Summary

```mermaid
flowchart TD
    T1["Timer1 COMPA ISR\n4 kHz / 250 µs"] --> RA["sr.readAll()\nCapture all 74HC165 inputs\n+ 8-sample debounce"]
    RA --> FL["Flipper::tick()\nLeft & Right\nidle→kick→hold PWM"]
    RA --> LS["LaunchSolenoid::tick()\nRising edge → 1 s fire"]
    RA --> DT["DropTargetBank::tick()\n+10/target · +50 full-clear\n3 s wait → 1 s reset pulse"]
    RA --> ED["Edge Detection\nrise0 / rise1 / rise2"]
    ED --> SEQ["Mode Sequence\nTop Left→Ball Entry→Right Lane\ntoggles negativeMode"]
    ED --> TL["Top Lanes\nF/Y rollovers · flipper rotates\nboth lit → jackpotFlag +50"]
    ED --> HU["Hurry-Up\nRight Lane opens 5 s window\nQuick Shot → addScoreFlag +100"]
    ED --> RM["Ramp\nIn2.0 rising → rampLaneFlag +10"]
    FL --> WA["sr.writeAll()\nPush shadow buffer\nto TPIC6C596 chain"]
    LS --> WA
    DT --> WA
    TL --> WA
    HU --> WA
    WA --> ML["Main Loop\nConsumes flags\nAPPLY_SCORE\n± negativeMode"]
    ML --> RS["usart.sendScore()\nRS485 → Score Board"]
```
