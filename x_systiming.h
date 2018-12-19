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

#include	<stdint.h>

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// #################################################################################################

#define	clockACT_TEST				0
#define	buildTICKTIMER_STATS		1

#define	IF_TICKTIMER_START(x,y)		if (x) xTickTimerStart(y)
#define	IF_TICKTIMER_STOP(x,y)		if (x) xTickTimerStop(y)
#define	IF_TICKTIMER_SHOW(x,y,z)	if (x) vTickTimerShow(y, z)
#define	IF_TICKTIMER_RESET(x,y)		if (x) vTickTimerReset(y)

#define	IF_CLOCKTIMER_START(x,y)	if (x) xClockTimerStart(y)
#define	IF_CLOCKTIMER_STOP(x,y)		if (x) xClockTimerStop(y)
#define	IF_CLOCKTIMER_SHOW(x,y,z)	if (x) vClockTimerShow(y, z)
#define	IF_CLOCKTIMER_RESET(x,y)	if (x) vClockTimerReset(y)

// ################################# Process timer support #########################################

enum {													// clockTIMERs
	clockACT_S0,
	clockACT_S1,
	clockACT_S2,
	clockACT_S3,
	clockACT_SX,
	clockACT_I2C,
	clockTIMER_SSD1306,
	clockTIMER_SSD1306_2,
	clockTIMER_M90EX6,
	clockTIMER_DS2482,
	clockTIMER_NUM,										// last in list, define all required above here
} ;

enum {													// tickTIMERs
	tickTIMER_L2,										// track L2 disconnected time & occurrences
	tickTIMER_L3,										// track L3 disconnected time & occurrences
	tickTIMER_DS2482,
//	tickTIMER_TFTP,										// TFTP task execution timing...
	tickTIMER_NUM,										// last in list, define all required above here
} ;

typedef struct ticktime_s {
	uint32_t	Sum ;
	uint32_t	Last ;
	uint32_t	Count ;
#if		(buildTICKTIMER_STATS == 1)
	uint32_t	Min ;
	uint32_t	Max ;
#endif
} ticktime_t ;

typedef struct clocktime_s {
	uint64_t	Sum ;
	uint32_t	Last ;
	uint32_t	Count ;
#if		(buildTICKTIMER_STATS == 1)
	uint32_t	Min ;
	uint32_t	Max ;
#endif
} clocktime_t ;

void	vTickTimerReset(uint32_t TimerMask) ;
uint32_t xTickTimerStart(uint8_t tNumber) ;
uint32_t xTickTimerStop(uint8_t tNumber) ;
uint32_t xTickTimerIsRunning(uint8_t TimNum) ;
uint32_t xTickTimerGetElapsedMillis(uint8_t TimNum) ;
uint32_t xTickTimerGetElapsedSecs(uint8_t TimNum) ;
void	vTickTimerGetStatus(uint8_t TimNum, ticktime_t * pTT) ;
void	vTickTimerShow(int32_t Handle, uint32_t TimerMask) ;

uint32_t xClockDelayUsec(uint32_t uSec) ;
uint32_t xClockDelayMsec(uint32_t mSec) ;

void	vClockTimerReset(uint8_t TimNum) ;
uint32_t xClockTimerStart(uint8_t TimNum) ;
uint32_t xClockTimerStop(uint8_t TimNum) ;
uint32_t xClockTimerIsRunning(uint8_t TimNum) ;
uint64_t xClockTimerGetElapsedClocks(uint8_t TimNum) ;
uint64_t xClockTimerGetElapsedMicros(uint8_t TimNum) ;
uint64_t xClockTimerGetElapsedMillis(uint8_t TimNum) ;
uint64_t xClockTimerGetElapsedSecs(uint8_t TimNum) ;
void	vClockTimerGetStatus(uint8_t TimNum, clocktime_t * pCT) ;
void	vClockTimerShow(int32_t Handle, uint32_t TimerMask) ;

void	vSysTimingTest(void) ;

#ifdef __cplusplus
}
#endif
