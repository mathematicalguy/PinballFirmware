/**
 * RS485_USART.cpp
 * EECE 5141C/6041C - Introduction to Mechatronics
 * Lab 8: USART Communication over RS485
 *
 * Implementation of the RS485_USART class for the ATmega328P.
 *
 * Clock:      16 MHz (Arduino Uno R3)
 * Baud:       250,000 bps  |  UBRR = (F_CPU / (16 * BAUD)) - 1 = 3
 * Frame:      9N1 (9 data bits, no parity, 1 stop bit)
 * Mode:       USART Multiprocessor Communication Mode (MPCM0 = 1 on receiver)
 */

#include "RS485_USART.h"

// ---------------------------------------------------------------------------
// USART baud-rate constant (ATmega328P datasheet eq. for normal async mode)
// F_CPU = 16 000 000 Hz, BAUD = 250 000 bps  |  UBRR = 3
// ---------------------------------------------------------------------------
#define BAUD_PRESCALE   3

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

RS485_USART::RS485_USART()
    : _isMaster(false),
      _score(0),
      _bufferIndex(0),
      _isrCount(0),
      _timerSubCount(0),
      _usartState(0),
      _usartAddress(0),
      _scoreLow(0)
{
    // Initialise the tx buffer with the static protocol bytes.
    // Elements [2] and [4] hold the live score bytes and are updated later.
    _txBuffer[0] = 0x10; // Lower 8 bits of node address (0x110 ? bit8 set separately)
    _txBuffer[1] = 0x01; // Lower score byte internal address
    _txBuffer[2] = 0x00; // Lower score byte (updated before each transmission)
    _txBuffer[3] = 0x02; // Upper score byte internal address
    _txBuffer[4] = 0x00; // Upper score byte (updated before each transmission)
    _txBuffer[5] = 0xFF; // Lower 8 bits of stop packet (0x1FF ? bit8 set separately)
}

// ---------------------------------------------------------------------------
// begin()  –  hardware initialisation
// ---------------------------------------------------------------------------

void RS485_USART::begin(bool isMaster)
{
    _isMaster = isMaster;

    // Configure RTS GPIO (PD2) as output, default LOW (receive mode).
    RTS_DDR  |=  (1 << RTS_PIN);
    RTS_PORT &= ~(1 << RTS_PIN);

    _configUSART();

    if (_isMaster) {
        // Transmitter: stay in transmit mode throughout; TX Complete ISR
        // is enabled on demand at the start of each 6-byte burst.
        _enableTransmit();

        // -----------------------------------------------------------------
        // Timer 2 – CTC mode, fires every ~10 ms (3 sub-ticks = ~30 ms)
        // F_CPU = 16 MHz, prescaler = 1024
        // OCR2A = (16 000 000 / 1024 / 100) - 1 = 155  ? ~100 Hz per tick
        // Timer 1 is reserved for the flipper ISR.
        // -----------------------------------------------------------------
        TCCR2A = (1 << WGM21);            // CTC mode (TOP = OCR2A)
        TCCR2B = (1 << CS22)              // prescaler = 1024
               | (1 << CS21)
               | (1 << CS20);
        OCR2A  = 155;
        TIMSK2 = (1 << OCIE2A);           // enable Output Compare A interrupt
    } else {
        // Receiver: enable Multiprocessor Communication Mode so that frames
        // whose 9th bit is 0 (data frames) are ignored until an address frame
        // matching our node is detected.
        UCSR0A |= (1 << MPCM0);

        // Enable the RX-complete interrupt.
        UCSR0B |= (1 << RXCIE0);

        // Receiver stays in receive mode (RTS low, already set above).
    }

    sei(); // Enable global interrupts
}

// ---------------------------------------------------------------------------
// _configUSART()  –  registers common to both roles
// ---------------------------------------------------------------------------

void RS485_USART::_configUSART()
{
    // Set baud rate (UBRR = 3 for 250 kBaud @ 16 MHz)
    UBRR0H = (uint8_t)(BAUD_PRESCALE >> 8);
    UBRR0L = (uint8_t)(BAUD_PRESCALE);

    // UCSR0A: normal async mode (U2X0 = 0, MPCM0 = 0 for transmitter)
    UCSR0A = 0;

    // UCSR0B:
    //   UCSZ02 = 1 | 9-bit character size (together with UCSZ01:00 = 11)
    //   TXEN0  = 1 | enable transmitter
    //   RXEN0  = 1 | enable receiver
    //   TX/RX complete ISRs are configured per-role after this call.
    UCSR0B = (1 << UCSZ02)
           | (1 << TXEN0)
           | (1 << RXEN0);

    // UCSR0C:
    //   UMSEL01:00 = 00 | asynchronous USART
    //   UPM01:00   = 00 | no parity
    //   USBS0      = 0  | 1 stop bit
    //   UCSZ01:00  = 11 | 9-bit character size (bit 2 is in UCSR0B above)
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

// ---------------------------------------------------------------------------
// _write9Bit()  –  transmit a 9-bit word
// ---------------------------------------------------------------------------

void RS485_USART::_write9Bit(uint16_t data)
{
    // Wait until the transmit buffer is empty (UDREn flag).
    // In ISR context this should already be true, but guard anyway.
    while (!(UCSR0A & (1 << UDRE0)));

    if (data & 0x0100) {
        UCSR0B |=  (1 << TXB80);   // 9th bit = 1 (address or stop frame)
    } else {
        UCSR0B &= ~(1 << TXB80);   // 9th bit = 0 (data frame)
    }

    UDR0 = (uint8_t)(data & 0x00FF); // write low 8 bits ? starts transmission
}

// ---------------------------------------------------------------------------
// _updateTxBuffer()  –  refresh score bytes in the tx buffer
// ---------------------------------------------------------------------------

void RS485_USART::_updateTxBuffer()
{
    _txBuffer[2] = (uint8_t)( _score & 0x00FF);        // lower byte
    _txBuffer[4] = (uint8_t)((_score >> 8) & 0x00FF);  // upper byte
}

// ---------------------------------------------------------------------------
// timerISR()  –  called from ISR(TIMER1_COMPA_vect) every 30 ms
// ---------------------------------------------------------------------------

void RS485_USART::timerISR()
{
    // ---- Sub-tick gate: Timer2 fires ~100 Hz; run logic every 3rd tick (~30 ms)
    if (++_timerSubCount < 3) return;
    _timerSubCount = 0;

    // ---- Score update gate (every 5th call = 150 ms) ----------------------
    _isrCount++;
    if (_isrCount >= SCORE_UPDATE_INTERVAL) {
        _score++;
        _isrCount = 0;
        _updateTxBuffer(); // keep buffer in sync with new score
    }

    // ---- Kick off a new 6-byte transmission --------------------------------
    // Send the first byte (node address frame: bit8 = 1, value = 0x10).
    _bufferIndex = 1;           // TX Complete ISR will continue from index 1

    _enableTxISR();             // arm the TX Complete ISR for subsequent bytes
    _write9Bit(SCOREBOARD_NODE_ADDR); // transmit address frame (bit8 = 1)
}

// ---------------------------------------------------------------------------
// txCompleteISR()  –  called from ISR(USART_TX_vect) after each byte
// ---------------------------------------------------------------------------

void RS485_USART::txCompleteISR()
{
    if (_bufferIndex < (TX_BUFFER_SIZE - 1)) {
        // ---- Bytes 1-4: data frames (bit8 = 0) ----------------------------
        // Addresses (indices 1, 3) and score bytes (indices 2, 4) are all
        // data frames with the 9th bit clear.
        _write9Bit((uint16_t)_txBuffer[_bufferIndex]); // bit8 = 0 (data frame)
        _bufferIndex++;

    } else {
        // ---- Last byte: stop packet (bit8 = 1) ----------------------------
        _disableTxISR();                // no more ISR triggers after this byte
        _write9Bit(STOP_PACKET);        // transmit stop frame (bit8 = 1)

    }
}

// ---------------------------------------------------------------------------
// rxCompleteISR()  –  called from ISR(USART_RX_vect) on the score board
// ---------------------------------------------------------------------------

void RS485_USART::rxCompleteISR()
{
    // Read the 9th (address) bit before reading UDR0 (clears the flag).
    uint8_t isAddress = (UCSR0B >> RXB80) & 0x01;
    uint8_t data      = UDR0;

    // ---- Address frame handling -------------------------------------------
    if (isAddress) {
        if (data == (SCOREBOARD_NODE_ADDR & 0x00FF)) {
            // Address matches our node ? disable MPCM so subsequent data
            // frames (bit8 = 0) are passed through to the RX buffer.
            UCSR0A     &= ~(1 << MPCM0);
            _usartState = 0; // reset inner state machine
        } else {
            // Address is for another node ? re-enable MPCM to ignore data.
            UCSR0A    |=  (1 << MPCM0);
            _usartState = 0;
        }
        return; // address frame fully handled
    }

    // ---- Data frame handling (MPCM has been cleared for our node) ---------
    switch (_usartState) {

        // -- State 0: expecting an internal register address ----------------
        case 0:
            _usartAddress = data;
            _usartState   = 1;
            break;

        // -- State 1: expecting the data byte for _usartAddress -------------
        case 1:
            switch (_usartAddress) {

                case 0x01: // lower score byte
                    _scoreLow   = data;
                    break;

                case 0x02: // upper score byte
                {
                    uint8_t scoreHigh = data;
                    _score = ((uint16_t)scoreHigh << 8) | _scoreLow;
                    break;
                }

                default:
                    // Unknown address – ignore and reset.
                    break;
            }
            _usartState = 0; // return to waiting for next internal address
            break;

        default:
            _usartState = 0;
            break;
    }
}

// ---------------------------------------------------------------------------
// Global ISR linkage
// The global instance is declared extern in the header; define it here.
// ---------------------------------------------------------------------------

RS485_USART usart;

/** Timer 2 Compare A ISR – fires every ~10 ms; logic runs every 3rd tick (~30 ms). */
ISR(TIMER2_COMPA_vect)
{
    usart.timerISR();
}

/** USART0 TX Complete ISR – fires on the transmitter after each byte. */
ISR(USART_TX_vect)
{
    usart.txCompleteISR();
}

/** USART0 RX Complete ISR – fires on the receiver after each byte. */
ISR(USART_RX_vect)
{
    usart.rxCompleteISR();
}