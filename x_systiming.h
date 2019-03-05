/*
 * Copyright 2014-18 Andre M Maree / KSS Technologies (Pty) Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/*
 * x_systiming.h
 */

#include	"freertos/FreeRTOSConfig.h"

#include	<stdint.h>

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// #################################################################################################

#define	systimerSCATTER							1			// enable scatter graphs support
#define	systimerSCATTER_GROUPS					10			// number of scatter graph groupings
#define	systimerSCATTER_HDR_SHOW				0			// 0=no scatter heading ie intelligent display

#define	IF_SYSTIMER_START(t,y)					if (t) xSysTimerStart(y)
#define	IF_SYSTIMER_STOP(t,y)					if (t) xSysTimerStop(y)

#define	IF_SYSTIMER_SHOW(t,h,m)					if (t) vSysTimerShow(h, m)
#define	IF_SYSTIMER_SHOW_NUM(t,h,n)				if (t) vSysTimerShow(h, 1 << n)

#define	IF_SYSTIMER_RESET(t,m,T,Tag, ...)		if (t) vSysTimerReset(m,T,Tag, ##__VA_ARGS__ )
#define	IF_SYSTIMER_RESET_NUM(t,n,T,Tag, ...)	if (t) vSysTimerReset(1UL<<n,T,Tag, ##__VA_ARGS__  )

// ################################# Process timer support #########################################

enum { systimerTICKS, systimerCLOCKS } ;

enum {
	systimerL2, 		systimerL3,						// track Lx disconnected time & occurrences
	systimerMQTT_RX,	systimerMQTT_TX,
//	systimerHTTP,
//	systimerFOTA,
//	systimerSLOG,
//	systimerPCA9555,
//	systimerDS2482,
//	systimerM90EX6,
//	systimerACT_S0, systimerACT_S1, systimerACT_S2, systimerACT_S3, systimerACT_SX,
//	systimerSSD1306, systimerSSD1306_2,
//	systimerTFTP,										// TFTP task execution timing...
	systimerMAX_NUM,									// last in list, define all required above here

	systimerHTTP = 31,
	systimerFOTA = 31,
	systimerSLOG = 31,
	systimerPCA9555 = 31,
	systimerDS2482 = 31,
	systimerM90EX6 = 31,
	systimerACT_S0 = 31, systimerACT_S1 = 31, systimerACT_S2 = 31, systimerACT_S3 = 31, systimerACT_SX = 31,
	systimerSSD1306 = 31, systimerSSD1306_2 = 31,
	systimerTFTP = 31,
} ;

typedef struct systimer_s {
	const char * Tag ;
	uint64_t	Sum ;
	uint32_t	Last, Count, Min, Max ;
#if		(systimerSCATTER == 1)
	uint32_t	SGmin, SGmax ;
	uint32_t	Group[systimerSCATTER_GROUPS] ;
#endif
} systimer_t ;

void	vSysTimerReset(uint32_t TimerMask, bool Type, const char * Tag, ...) ;
uint32_t xSysTimerStart(uint8_t tNumber) ;
uint32_t xSysTimerStop(uint8_t tNumber) ;
uint32_t xSysTimerIsRunning(uint8_t TimNum) ;
bool	bSysTimerGetStatus(uint8_t TimNum, systimer_t *) ;

uint64_t xSysTimerGetElapsedClocks(uint8_t TimNum) ;
uint64_t xSysTimerGetElapsedMicros(uint8_t TimNum) ;
uint64_t xSysTimerGetElapsedMillis(uint8_t TimNum) ;
uint64_t xSysTimerGetElapsedSecs(uint8_t TimNum) ;

void	vSysTimerShow(int32_t Handle, uint32_t TimerMask) ;

uint32_t xClockDelayUsec(uint32_t uSec) ;
uint32_t xClockDelayMsec(uint32_t mSec) ;

void	vSysTimingTest(void) ;

#ifdef __cplusplus
}
#endif
