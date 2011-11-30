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

//-----------------------------------------------------------------------------
// Radio packet blaster
//-----------------------------------------------------------------------------

#include "stdmansos.h"
#include <lib/random.h>

char sendBuffer[] = "hello world";

void radioRecvCb(void)  {
    static uint8_t buffer[128];
    int16_t len;
    toggleRedLed();
    len = radioRecv(buffer, sizeof(buffer) - 1);
    if (len <= 0) {
        PRINTF("radio recv failed, len=%d\n", len);
        return;
    }
    if (len > 0) {
        buffer[len] = 0;
        PRINTF("recv: %s\n", (char *) buffer);
    }
}

void appMain(void)
{
    randomInit();
    radioSetReceiveHandle(radioRecvCb);
    radioOn();
    for (;;) {
        toggleGreenLed();
        radioSend(sendBuffer, sizeof(sendBuffer));
        mdelay(600 + randomRand() % 400);
    }
}