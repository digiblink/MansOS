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

//===========================================================================
// 3D Accelerometer test on a custom msp platorm
//===========================================================================

#include "stdmansos.h"
#include "scheduler.h"
#include "devmgr.h"
#include "drivers/dev_sen_accel3D.h"

#include <string.h>
#include <stdio.h>

#include "dprint.h"

//===========================================
//  Constants
//===========================================

// Accelerometer ADC ports
#define ACC_X_PORT 0
#define ACC_Y_PORT 1
#define ACC_Z_PORT 2


//-------------------------------------------
//      Entry point for the application
//-------------------------------------------
void appMain(void)
{
    static devMgrErr_t ret;
    accelVector3D_t accel;

    devParams_t params;
    params.data = (void*) &accel;

    devParams_t adcParams;
    static uint16_t adcVal;
    adcParams.data = (char *) &adcVal;

    PRINT_INIT(128);

    //Turn the Accelerometer on (P4.0=1)!
    PIN_AS_DATA(4,0);
    PIN_AS_OUTPUT(4,0);
    PIN_SET(4,0);

    adcSetChannel(1);
    ADC12MCTL2 = SREF_AVCC_AVSS;

    while(1)
    {
        //ret = devCall(DEV_SEN_ACCEL, 0, DMF_READ, &params);

        ret = devCall(DEV_ADC, ACC_X_PORT, DMF_READ, &adcParams);
        accel.x = adcVal;
        threadSleep(10);
        ret = devCall(DEV_ADC, ACC_Y_PORT, DMF_READ, &adcParams);
        accel.y = adcVal;
        threadSleep(10);
        ret = devCall(DEV_ADC, ACC_Z_PORT, DMF_READ, &adcParams);
        accel.z = adcVal;
        threadSleep(10);

        PRINTF("3D Accel: X= %4u  Y= %4u  Z= %4u \n",
            accel.x, accel.y, accel.z);

        threadSleep(10);
    }
}
