/**
 * Copyright (c) 2008-2010 Leo Selavo and the contributors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of  conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "common.h"
#include <hil/fstream.h>
#include <hil/extflash.h>

#define DO_FLASH_LOGGING 1

#if 0
#define RPRINTF PRINTF
#define rdebugHexdump debugHexdump
#else
#define RPRINTF(...) // nothing
#define rdebugHexdump(...) // nothing
#endif

void packetRx(Packet_t *);

// this should also account for preamble + frame delimiter size
uint8_t rxBuffer[MAX_FRAME_SIZE * MAX_SAMPLES_PER_BIT / 8];

uint8_t rxDataBuffer[MAX_PACKET_SIZE];
uint16_t rxDataBitNum;
uint16_t rxDataZeroBits;

//
// timer A interrupt
//
interrupt (TIMERA1_VECTOR) timerA1Int()
{
    if (ALARM_TIMER_EXPIRED())
    {
        // increment the 'real time' value
        extern uint32_t realTime;
        ++realTime;
    }
}

static inline void rxDataReset()
{
    rxDataBitNum = 0;
    rxDataZeroBits = 0;
}

// returns true on end of packet
static inline bool rxDataBit(uint8_t bit)
{
    if (bit) {
        if (rxDataZeroBits < MAX_SUCCESSIVE_ZERO_BITS) {
            // ignore it
            rxDataZeroBits = 0;
            return false;
        }
        rxDataZeroBits = 0;
        rxDataBuffer[rxDataBitNum / 8] |= (1 << (rxDataBitNum % 8));
    } else {
        enum { ENDING_ZERO_BITS = 5 };
        if (++rxDataZeroBits >= ENDING_ZERO_BITS) return true;
        rxDataBuffer[rxDataBitNum / 8] &= ~(1 << (rxDataBitNum % 8));
    }
    ++rxDataBitNum;
    return (rxDataBitNum >= sizeof(rxDataBuffer) * 8);
}


static inline bool rxDataBitMe(uint8_t bit)
{
    if (bit) rxDataBuffer[rxDataBitNum / 8] |= (1 << (rxDataBitNum % 8));
    else rxDataBuffer[rxDataBitNum / 8] &= ~(1 << (rxDataBitNum % 8));

    ++rxDataBitNum;
    return (rxDataBitNum >= sizeof(rxDataBuffer) * 8);
}

static void rxDataProcess()
{
    // cut it down to byte boundary
    uint16_t length = rxDataBitNum / 8;

    RPRINTF("a packet sucessfully received!\n");
    rdebugHexdump(rxDataBuffer, length);

#if ENCODING_HAMMING
    length = hammingDecodeInplace(rxDataBuffer, length);
    if (length) {
        RPRINTF("packet after decoding: \n");
        rdebugHexdump(rxDataBuffer, length);
    } else {
        RPRINTF("Hamming decoding failed\n");
    }
#endif

    if (length != sizeof(Frame_t)) {
        RPRINTF("bad length: %u\n", length);
        return;
    }
    
    Frame_t *frame = (Frame_t *) rxDataBuffer;
    if (crc16((uint8_t *) frame, sizeof(frame->data)) != frame->crc) {
        RPRINTF("bad checksum\n");
        return;
    }

    packetRx(&frame->data);
}

static inline void decodeOctet(uint8_t octet, uint8_t byteArray[8])
{
    byteArray[0] = (octet & (1 << 0)) ? 1 : 0;
    byteArray[1] = (octet & (1 << 1)) ? 1 : 0;
    byteArray[2] = (octet & (1 << 2)) ? 1 : 0;
    byteArray[3] = (octet & (1 << 3)) ? 1 : 0;
    byteArray[4] = (octet & (1 << 4)) ? 1 : 0;
    byteArray[5] = (octet & (1 << 5)) ? 1 : 0;
    byteArray[6] = (octet & (1 << 6)) ? 1 : 0;
    byteArray[7] = (octet & (1 << 7)) ? 1 : 0;
}

// how many samples should be 1 to detect bit "1"?
const uint8_t BIT_SAMPLE_COUNT[MAX_SAMPLES_PER_BIT + 1] =
{
    0, // 0
    1, // 1
    2, // 2
    2, // 3
    3, // 4
    3, // 5
    4, // 6
    4, // 7
    5, // 8
    5, // 9
    6 // 10
};

enum { BYTE_ARRAY_SIZE = 32 };

// XXX: this could be optimized
static inline uint8_t detectBit(uint8_t *array, uint16_t arrayPos,
        uint16_t samplesPerBit)
{
    uint16_t numOnes = 0;
    uint16_t i;
    for (i = 0; i < samplesPerBit; ++i) {
        numOnes += array[(arrayPos + i) % BYTE_ARRAY_SIZE];
    }
    return numOnes >= BIT_SAMPLE_COUNT[samplesPerBit];
}

static inline uint8_t detectMeBit(uint8_t *array, uint16_t arrayPos, bool *over)
{
    uint16_t numOnes1 = 0, numOnes2 = 0;
    uint16_t i = (arrayPos + 1) % BYTE_ARRAY_SIZE;
    numOnes1 += array[i++];
    i %= BYTE_ARRAY_SIZE;
    numOnes1 += array[i++];
    i %= BYTE_ARRAY_SIZE;
    numOnes1 += array[i++];
    i %= BYTE_ARRAY_SIZE;
    numOnes1 += array[i++];
    i %= BYTE_ARRAY_SIZE;
    numOnes1 += array[i++];
    i %= BYTE_ARRAY_SIZE;
    numOnes1 += array[i];
    i = (arrayPos + SAMPLES_PER_BIT + 1) % BYTE_ARRAY_SIZE;
    numOnes2 += array[i++];
    i %= BYTE_ARRAY_SIZE;
    numOnes2 += array[i++];
    i %= BYTE_ARRAY_SIZE;
    numOnes2 += array[i++];
    i %= BYTE_ARRAY_SIZE;
    numOnes2 += array[i++];
    i %= BYTE_ARRAY_SIZE;
    numOnes2 += array[i++];
    i %= BYTE_ARRAY_SIZE;
    numOnes2 += array[i];
    // the frame ends with all ones (but count zeros as ending as well)
    *over = ((numOnes1 >= 5 && numOnes2 >= 5)
            || (numOnes1 <= 0 && numOnes2 <= 0));
    return numOnes1 > numOnes2;
}

static inline uint16_t detectEdge(uint8_t *array, uint16_t offset, uint8_t bit)
{
    uint16_t score = 0;
    uint8_t *p = array + offset;
    if (*p++ == bit) ++score;
    if (*p++ == bit) ++score;
    if (*p++ != bit) ++score;
    if (*p++ != bit) ++score;
    return score;
}

// XXX: runningBitLength is not adjusted here
static bool detectDelimiterBit(uint8_t *array, uint16_t *arrayPos,
        uint16_t *runningBitLength, uint16_t delimiterBitNum) 
{
    const uint8_t expectedBit = delimiterBitNum & 0x1;

    // now detect the edge
    uint8_t edge[8];
    uint16_t i, idx;
    idx = (uint16_t)(*arrayPos - 4) % BYTE_ARRAY_SIZE;
    for (i = 0; i < 8; ++i) {
        idx = (idx + 1) % BYTE_ARRAY_SIZE;
        edge[i] = array[idx];
    }
    uint16_t bestScore = 0;
    uint16_t bestPos = 0;
    for (i = 0; i < 5; ++i) {
        uint16_t score = detectEdge(edge, i, 1 ^ expectedBit);
        if (score > bestScore) {
            bestScore = score;
            bestPos = i;
        }
    }
    RPRINTF("detectDelimiterBit: bestPos=%u, bestScore=%u\n", bestPos, bestScore);
    rdebugHexdump(edge, sizeof(edge));

    *arrayPos = (uint16_t)(*arrayPos + bestPos - 2) % BYTE_ARRAY_SIZE;
    return true;
}

#if ENCODING_MANCHESTER
static uint8_t detectDataBitMe(uint8_t *array, uint16_t *arrayPos,
        uint16_t runningBitLength, bool *over) 
{
    uint8_t result = detectMeBit(array, *arrayPos, over);
    if (*over) {
        //RPRINTF("detectDataBitMe: over!\n");
        return 0;
    }

    // now detect the edge
    uint8_t edge[8];
    uint16_t i, idx;
    idx = (*arrayPos + (uint16_t)(runningBitLength - 4)) % BYTE_ARRAY_SIZE;
    for (i = 0; i < 8; ++i) {
        idx = (idx + 1) % BYTE_ARRAY_SIZE;
        edge[i] = array[idx];
    }
    uint16_t bestScore = 0;
    uint16_t bestPos = 0;
    for (i = 0; i < 5; ++i) {
        uint16_t score = detectEdge(edge, i, result);
        if (score >= bestScore) {
            bestScore = score;
            bestPos = i;
        }
    }

    *arrayPos = (*arrayPos + (uint16_t)(2 * runningBitLength + bestPos - 2))
            % BYTE_ARRAY_SIZE;
    return result;
}

#else // !ENCODING_MANCHESTER

static uint8_t detectDataBit(uint8_t *array, uint16_t *arrayPos,
        uint16_t runningBitLength) 
{
    uint16_t startIndex = *arrayPos;
    uint8_t result = detectBit(array, startIndex, runningBitLength);

    uint16_t endIndex = (startIndex + runningBitLength - 1) % BYTE_ARRAY_SIZE;
    if (array[endIndex] != result) {
        startIndex = (uint16_t)(startIndex - 1) % BYTE_ARRAY_SIZE;
        if (array[startIndex] == result) {
            // move it back
            *arrayPos = startIndex;
            //PRINT("DDtB: move frame back\n");
        }
    } else {
        endIndex = (endIndex + 1) % BYTE_ARRAY_SIZE;
        if (array[endIndex] == result) {
            if (array[startIndex] != result) {
                *arrayPos = (startIndex + 1) % BYTE_ARRAY_SIZE;
                //PRINT("DDtB: move frame forward\n");
            }
        }
    }

    return result;
}

#endif // ENCODING_MANCHESTER

#if ENCODING_MANCHESTER
#define MAX_SAMPLES() (SAMPLES_PER_BIT * 2 + 4)
#else
#define MAX_SAMPLES() (MAX_SAMPLES_PER_BIT + 1)
#endif
    
#define ENOUGH_SPACE(readPos, writePos)                                 \
    ((writePos > readPos) ?                                             \
            (writePos >= readPos + MAX_SAMPLES()) :                     \
            (writePos + BYTE_ARRAY_SIZE >= readPos + MAX_SAMPLES()))

//
// Take the packet recieved and convert it from samples to bits.
// End of packet is signaled by n zero bits 
// (count depends on whether Manch. Enc. is used)
// The precise ending is determined by the end of the last octet.
//
void processPacket()
{
    // at this moment the rx buffer contains delimiter sequence,
    // the data packet, and some zero samples in the end.
    RPRINTF("%lu: a potential packet received\n", getRealTime());
    rdebugHexdump(rxBuffer, 256);

    // start with default model parameters
    uint16_t runningBitLength = SAMPLES_PER_BIT;
    enum {
        STATE_PREAMBLE,
        STATE_DELIM,
        STATE_DATA,
    } state = STATE_PREAMBLE;
    uint16_t delimiterBit = 0;
    uint16_t delimiterStartPos = 0;

    // decode 4 octets we are currently processing in this array
    static uint8_t byteArray[BYTE_ARRAY_SIZE];
    uint16_t writePos, readPos;

    uint8_t *pbuf = rxBuffer;
    const uint8_t *pend = pbuf + sizeof(rxBuffer);
    decodeOctet(*pbuf++, byteArray);
    decodeOctet(*pbuf++, byteArray + 8);
    decodeOctet(*pbuf++, byteArray + 16);
    decodeOctet(*pbuf++, byteArray + 24);

    // gap sequence must precede everything else
    if (detectBit(byteArray, 0, SAMPLES_PER_BIT) != 1
            || detectBit(byteArray, SAMPLES_PER_BIT, SAMPLES_PER_BIT) != 1) {
        RPRINTF("the packet does not start with preamble!\n");
        return;
    }

    writePos = 0; // (this much has been decoded) mod 32
    readPos = 16; // this much has been processed

    for (; pbuf != pend; ++pbuf) {
        decodeOctet(*pbuf, byteArray + writePos);
        writePos = (writePos + 8) % sizeof(byteArray);

        // there may be 0, 1, or 2 inner 'while' loops per 'for' loop
        while (ENOUGH_SPACE(readPos, writePos)) {
            switch (state) {
            case STATE_PREAMBLE:
                if (detectBit(byteArray, readPos, SAMPLES_PER_BIT) != 0) {
                    // nothing good here yet.
                    break;
                }
                state = STATE_DELIM;
                // fallthrough
            case STATE_DELIM:
                if (!detectDelimiterBit(byteArray, &readPos,
                                &runningBitLength, delimiterBit)) {
                    // delimiter broken, cannot proceed
                    return;
                }
                ++delimiterBit;
                if (delimiterBit == 2) {
                    delimiterStartPos = readPos;
                }
                else if (delimiterBit == FRAME_DELIM_SIZE) {
                    uint16_t delimSeqLen;
                    delimSeqLen = ((uint16_t) (readPos - delimiterStartPos))
                            % BYTE_ARRAY_SIZE;
                    if (delimSeqLen < 4 * MIN_SAMPLES_PER_BIT) {
                        delimSeqLen += BYTE_ARRAY_SIZE;
                    }
                    delimSeqLen += 2; // to round it correctly
                    //RPRINTF("processPacket: final adjust bit size, t=%u, size=%u\n",
                    //        delimSeqLen, delimSeqLen / 4);
                    runningBitLength = delimSeqLen / 4;

                    //PRINT("processPacket: process data\n");
                    state = STATE_DATA;
                    rxDataReset();
                }

                break;

            case STATE_DATA: {
#if ENCODING_MANCHESTER
                bool over;
                const uint8_t bit = detectDataBitMe(
                        byteArray, &readPos, runningBitLength, &over);
                if (over || rxDataBitMe(bit)) {
                    rxDataProcess();
                    return;
                }
                goto end;
#else
                if (rxDataBit(detectDataBit(
                                        byteArray, &readPos, runningBitLength))) {
                    rxDataProcess();
                    return;
                }
                break;
#endif
            }
            }
            readPos = (readPos + runningBitLength) % sizeof(byteArray);

          end:
            ;
        }
    }

    PRINT("processPacket: done.\n");
}

//
// The target is to have 8 samples for each bits.
// These delays are far from perfect - 
// depending on DCO frequency they can be too small or too large.
//
#define DELAY() { NOP10(); NOP10(); NOP10(); NOP6(); };
// this delay is used after the last sample;
// all the processing after it takes ~10 CPU cycles, so discount those.
#define DELAY_SMALL() { NOP10(); NOP10(); NOP6(); };

static inline uint8_t rxOctet() INLINE;
static inline uint8_t rxOctet()
{
    register uint16_t rxBits = 0;
    rxBits |= TRM433_READ_DATA_FAST() >> 3;
    DELAY();
    // add extra nops to compensate for different shifts
    NOP5();
    rxBits |= TRM433_READ_DATA_FAST() >> 2;
    DELAY();
    NOP2();
    rxBits |= TRM433_READ_DATA_FAST() >> 1;
    DELAY();
    NOP3();
    rxBits |= TRM433_READ_DATA_FAST();
    DELAY();
    NOP5();
    rxBits |= TRM433_READ_DATA_FAST() << 1;
    DELAY();
    NOP4();
    rxBits |= TRM433_READ_DATA_FAST() << 2;
    DELAY();
    NOP3();
    rxBits |= TRM433_READ_DATA_FAST() << 3;
    DELAY();
    NOP2();
    rxBits |= TRM433_READ_DATA_FAST() << 4;
    DELAY_SMALL();
    return rxBits;
}

static bool detectPreamble()
{
    enum { RSSI_THRESHOLD = 3000 };

    uint16_t rssi = adcReadFast();

    return rssi > RSSI_THRESHOLD;
}

void rxData()
{
    for (;;) {
        DISABLE_INTS();
        if (!detectPreamble()) {
            ENABLE_INTS();
            continue;
        }

        // -- read whole packet, as much as the buffer allows
        register uint16_t rxOctetNum = 0;
        for (; rxOctetNum < sizeof(rxBuffer); ++rxOctetNum) {
            rxBuffer[rxOctetNum] = rxOctet();
        }

        ENABLE_INTS();

        // -- now process the data in the buffer
        processPacket();
    }
}

// ------------------------------------
// flash writing stuff
// ------------------------------------

#define DATA_MAGIC_NUMBER  0xbaadf00dul

// all data types
enum {
    DB_STRING = 1, // just a string (not including zero character)
    DB_PACKET,     // a complete radio packet
    DB_ACCEL_GYRO, // only accel and gyro data
};

// data record superstructure
typedef struct Datablock {
    uint32_t magic;     // magic number, used to indentify start of datablock
    uint8_t dataType;   // one of the predefined data types
    uint8_t __padding1; // should be zero
    uint16_t blockLen;  // size of the block, including header
    // uint32_t timestamp; // time when this recond was made
    uint8_t data[];     // the data (variable size)
} Datablock_t PACKED;


bool flashWritePacket(Packet_t *p) {
    uint8_t buffer[100];
    Datablock_t *db;

    // write metainfo
    db = (Datablock_t *) buffer;
    db->magic = DATA_MAGIC_NUMBER;
    db->dataType = DB_PACKET;
    db->__padding1 = 0;
    db->blockLen = sizeof(Datablock_t) + sizeof(Packet_t);

    // write the packet
    memcpy(db->data, p, sizeof(*p));

    return flashStreamWrite(buffer, db->blockLen);
}

//
// receive and process the packet
//
void packetRx(Packet_t *pck)
{
    static uint32_t lastRxTimestamp;

    if (pck->version != PACKET_FORMAT_VERSION) {
        PRINTF("bad version: %u\n", pck->version);
        return;
    }

    // if it's a duplicate, ignore it
    if (pck->timestamp == lastRxTimestamp) return;

    PRINTF("got packet: originator=%u, light=%u, t=%u, act=%u, gps=(%lu %lu), time=%lu\n",
            pck->originator,
            pck->light,
            pck->temperature,
            pck->activity,
            pck->gpsLat,
            pck->gpsLon,
            pck->timestamp);
    lastRxTimestamp = pck->timestamp;

#if DO_FLASH_LOGGING
    // log the packet to external flash
    flashWritePacket(pck);
#endif

}

//-------------------------------------------
//      Entry point for the application
//-------------------------------------------
void appMain(void)
{
    PRINT_INIT(127);

    // initialize the external flash
    extFlashWake();

    TRM433_INIT();

    adcSetChannel(TRM433_RSSI_CH);

    TRM433_RX_MODE();
    TRM433_ON();

    rxData();
}