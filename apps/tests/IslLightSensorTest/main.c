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

//-------------------------------------------
//  Light sensor test, reads ISL29003 sensor
//  and prints to serial.
//-------------------------------------------
#include "mansos.h"
#include <hil/i2c_soft.h>
#include <isl29003/isl29003.h>

IslConfigure_t conf = {
    .mode = USE_BOUTH_DIODES,
    .clock_cycles = CLOCK_CYCLES_16,
    .range_gain = RANGE_GAIN_62,
    .integration_cycles = INTEGRATION_CYCLES_16,
};

//-------------------------------------------
//      Entry point for the application
//-------------------------------------------
void appMain(void)
{
    uint16_t islLight;
    islInit();
    islOn();
    configureIsl(conf);
    
    for (;;){
        // islRead(uint16_t *data, bool waitForInterupt),
        // returns true on success, false on fail.
        if (!islRead(&islLight, true)){
            PRINT("islRead failed\n");
        } else {
            PRINTF("islLight = %#x\n", islLight);
        }
        toggleRedLed();
    }
}