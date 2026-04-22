/**
 * RS485_USART.h
 * EECE 5141C/6041C - Introduction to Mechatronics
 * Lab 8: USART Communication over RS485
 *
 * Header for the RS485_USART class, which handles 9-bit Multiprocessor
 * Communication Mode USART on the ATmega328P (Arduino Uno R3).
 *
 * Frame format: 9 data bits, no parity, 1 stop bit (9N1)
 * Baud rate:    250,000 bps
 * Protocol:     RS485 half-duplex via external transceiver
 */

#ifndef RS485_USART_H
#define RS485_USART_H

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Pin & protocol constants
// ---------------------------------------------------------------------------

/** Arduino digital pin 2 (PD2) drives the transceiver RTS/DE line.
 *  HIGH = transmit mode, LOW = receive mode. */
#define RTS_PIN     PD2
#define RTS_PORT    PORTD
#define RTS_DDR     DDRD

/** Scoreboard node address (9-bit, 9th bit = 1 for address frames). */
#define SCOREBOARD_NODE_ADDR    0x110   ///< address frame: bit8=1, value=0x10

/** Internal register addresses sent as data frames (bit8 = 0). */
#define LOWER_BYTE_ADDR         0x001   ///< internal addr for lower score byte
#define UPPER_BYTE_ADDR         0x002   ///< internal addr for upper score byte

/** Stop packet: bit8=1 signals end-of-transmission. */
#define STOP_PACKET             0x1FF

/** Number of bytes in one complete score transmission. */
#define TX_BUFFER_SIZE          6

/** Timer 1 ISR fires every 30 ms; score updates every 5th fire (150 ms). */
#define SCORE_UPDATE_INTERVAL   5

// ---------------------------------------------------------------------------
// RS485_USART class
// ---------------------------------------------------------------------------

class RS485_USART {
public:
    // -----------------------------------------------------------------------
    // Construction / initialisation
    // -----------------------------------------------------------------------

    /** Default constructor – does not configure hardware. Call begin(). */
    RS485_USART();

    /**
     * Initialise USART0 for 9N1 Multiprocessor Communication Mode at 250 kBaud,
     * configure the RTS GPIO, and (for the receiver) enable the RX-complete ISR.
     *
     * @param isMaster  true  ? configure as transmitter (primary UNO board)
     *                  false ? configure as receiver (score board)
     */
    void begin(bool isMaster);

    // -----------------------------------------------------------------------
    // Transmitter API (primary UNO board)
    // -----------------------------------------------------------------------

    /**
     * Called from Timer 1 ISR every 30 ms.
     * Increments the internal counter, updates the score every 150 ms,
     * refreshes the tx buffer, and kicks off a new 6-byte transmission.
     */
    void timerISR();

    /**
     * Called from the USART0 TX-complete ISR.
     * Sends the next byte from the tx buffer, or terminates the
     * transmission if all bytes have been sent.
     */
    void txCompleteISR();

    // -----------------------------------------------------------------------
    // Receiver API (score board)
    // -----------------------------------------------------------------------

    /**
     * Called from the USART0 RX-complete ISR.
     * Implements the receiver state machine described in the lab document.
     */
    void rxCompleteISR();

    /**
     * Returns the current 16-bit score held by the receiver.
     * Safe to read from main loop (volatile).
     */
    uint16_t getScore() const { return _score; }

private:
    // -----------------------------------------------------------------------
    // Shared state
    // -----------------------------------------------------------------------
    bool        _isMaster;          ///< true = transmitter, false = receiver
    volatile uint16_t _score;       ///< current 16-bit score value

    // -----------------------------------------------------------------------
    // Transmitter state
    // -----------------------------------------------------------------------
    uint8_t     _txBuffer[TX_BUFFER_SIZE]; ///< 6-byte transmission sequence
    volatile uint8_t _bufferIndex;   ///< next byte to send from _txBuffer
    volatile uint8_t _isrCount;      ///< counts 30 ms ticks for 150 ms gate
    volatile uint8_t _timerSubCount; ///< counts Timer2 sub-ticks (3 × ~10 ms = ~30 ms)

    // -----------------------------------------------------------------------
    // Receiver state machine
    // -----------------------------------------------------------------------

    /** Outer state: 0 = waiting for internal address, 1 = waiting for data. */
    volatile uint8_t _usartState;

    /** Stores the most-recently received internal register address. */
    volatile uint8_t _usartAddress;

    /** Temporary storage for the lower score byte. */
    volatile uint8_t _scoreLow;

    // -----------------------------------------------------------------------
    // Private helpers
    // -----------------------------------------------------------------------

    /** Configure USART0 registers (shared by both roles). */
    void _configUSART();

    /** Set the RTS pin HIGH to enable the RS485 transceiver transmit driver. */
    inline void _enableTransmit()  { RTS_PORT |=  (1 << RTS_PIN); }

    /** Set the RTS pin LOW to release the RS485 bus (receive mode). */
    inline void _disableTransmit() { RTS_PORT &= ~(1 << RTS_PIN); }

    /** Enable the USART0 TX-complete interrupt. */
    inline void _enableTxISR()  { UCSR0B |=  (1 << TXCIE0); }

    /** Disable the USART0 TX-complete interrupt. */
    inline void _disableTxISR() { UCSR0B &= ~(1 << TXCIE0); }

    /**
     * Write a 9-bit word to the USART0 transmit buffer.
     * The 9th bit is placed in TXB80 before writing the low 8 bits to UDR0.
     *
     * @param data  9-bit value (bit 8 = address/stop flag, bits 7-0 = payload)
     */
    void _write9Bit(uint16_t data);

    /** Refresh _txBuffer[2] and _txBuffer[4] with the current score bytes. */
    void _updateTxBuffer();
};

// ---------------------------------------------------------------------------
// Global instance (defined in RS485_USART.cpp; extern here for ISR linkage)
// ---------------------------------------------------------------------------
extern RS485_USART usart;

#endif // RS485_USART_H