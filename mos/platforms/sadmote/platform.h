/*
 * Copyright (c) 2008-2012 the MansOS team. All rights reserved.
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

#ifndef _PLATFORM_SADMOTE_H_
#define _PLATFORM_SADMOTE_H_

#include <msp430/msp430_clock.h>
#include <msp430/msp430_timers.h>
#include <msp430/msp430_int.h>
#include <msp430/msp430_adc.h>
#include <msp430/msp430_usart.h>

#include "mrf24j40_pins.h"
#include "at25df_pins.h"
#include "sht_pins.h"

#ifdef USE_ISL29003
#include <isl29003/isl29003.h>
#endif
#ifdef USE_ADS1115
#include <ads1115/ads1115.h>
#endif

//===========================================================
// Functions
//===========================================================

void initPlatform(void);

//===========================================================
// Data types and constants
//===========================================================


#define EXT_FLASH_CHIP FLASH_CHIP_AT25DF

#define RADIO_CHIP RADIO_CHIP_MRF24J40

#define SNUM_CHIP SNUM_DS2401

// available USART count
#define SERIAL_COUNT 2
// use USART 1 for PRINTF
#define PRINTF_SERIAL_ID  1


// serial pins, for sw serial
#define UART0_TX_PORT 3
#define UART0_TX_PIN  4
#define UART0_RX_PORT 3
#define UART0_RX_PIN  5

#define UART1_TX_PORT 3
#define UART1_TX_PIN  6
#define UART1_RX_PORT 3
#define UART1_RX_PIN  7

#endif
