/**
 * Copyright (c) 2011, Institute of Electronics and Computer Science
 * All rights reserved.
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
 *
 * msp430_adc10.h -- ADC10 functionality on MSP430x2xx family
 */

#ifndef _MSP430_ADC10_H_
#define _MSP430_ADC10_H_

#include <kernel/defines.h>

#define ADC_CHANNEL_COUNT   16

/* Predefined ADC channels */
enum {
    ADC_INTERNAL_TEMPERATURE = 10,
    ADC_INTERNAL_VOLTAGE     = 11
};

/* Initialize ADC */
static inline void hplAdcInit(void)
{
    /*
     * SREF_1:      VR+ = VRef+ and VR- = Vss
     * ADC10SHT_3:  64 ADC10CLKs sample-and-hold time
     */
    ADC10CTL0 = ADC10SHT_3 | SREF_1;
}

/* Switch to 2.5V reference instead of 1.5V */
static inline void hplAdcUse2V5VRef(void)
{
    ADC10CTL0 |= REF2_5V;
}

/* Turn ADC on */
static inline void hplAdcOn(void)
{
    ADC10CTL0 |= REFON;     /* Turn on reference generator */
    ADC10CTL0 |= ADC10ON;   /* Turn on ADC core */
    ADC10CTL0 |= ENC;       /* Allow conversion */
}

/* Turn ADC off */
static inline void hplAdcOff(void)
{
    ADC10CTL0 &= ~ENC;
    ADC10CTL0 &= ~ADC10ON;
    ADC10CTL0 &= ~REFON;
}

/* Get converted value */
static inline uint16_t hplAdcGetVal(void)
{
    return ADC10MEM;
}

/* ADC channel count */
static inline unsigned adcGetChannelCount(void)
{
    return ADC_CHANNEL_COUNT;
}

/* Set ADC channel */
static inline void hplAdcSetChannel(unsigned ch)
{
    /* The channel number is stored in four MSBs of ADC10CTL1 */
    ADC10CTL1 = (ADC10CTL1 & 0x0FFFU) | (ch << 12);
}

/* Enable ADC interrupts */
static inline void hplAdcEnableInterrupt(void)
{
    ADC10CTL0 |= ADC10IE;
}

/* Disable ADC interrupts */
static inline void hplAdcDisableInterrupt(void)
{
    ADC10CTL0 &= ~ADC10IE;
}

/* Check if interrupts are enabled */
static inline bool hplAdcIntsUsed(void)
{
    return ADC10CTL0 & ADC10IE;
}

/* Start conversion */
static inline void hplAdcStartConversion(void)
{
    ADC10CTL0 |= ADC10SC;
}

/* Check if ADC is busy */
static inline bool hplAdcIsBusy(void)
{
    return ADC10CTL1 & ADC10BUSY;
}

#endif /* _MSP430_ADC10_H_ */