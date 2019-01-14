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

#define	IF_TICKTIMER_START(t,y)			if (t) xTickTimerStart(y)
#define	IF_TICKTIMER_STOP(t,y)			if (t) xTickTimerStop(y)
#define	IF_TICKTIMER_SHOW(t,y,z)		if (t) vTickTimerShow(y, z)
#define	IF_TICKTIMER_RESET_NUM(t,y)		if (t) vTickTimerReset(1 << y)
#define	IF_TICKTIMER_RESET(t,m)			if (t) vTickTimerReset(m)

#define	IF_CLOCKTIMER_START(t,y)		if (t) xClockTimerStart(y)
#define	IF_CLOCKTIMER_STOP(t,y)			if (t) xClockTimerStop(y)
#define	IF_CLOCKTIMER_SHOW(t,y,z)		if (t) vClockTimerShow(y, z)
#define	IF_CLOCKTIMER_RESET_NUM(t,y)	if (t) vClockTimerReset(1 << y)
#define	IF_CLOCKTIMER_RESET(t,m)		if (t) vClockTimerReset(m)

// ################################# Process timer support #########################################

enum {													// clockTIMERs
#if 0
	clockACT_S0,
	clockACT_S1,
	clockACT_S2,
	clockACT_S3,
	clockACT_SX,
	clockACT_I2C,
#else
	clockACT_S0	= 0,
	clockACT_S1 = 0,
	clockACT_S2 = 0,
	clockACT_S3 = 0,
	clockACT_SX = 0,
	clockACT_I2C = 0, 									// set all to ZERO ie overlap
#endif
#if 0
	clockTIMER_SSD1306,
	clockTIMER_SSD1306_2,
	clockTIMER_M90EX6,
#else
	clockTIMER_SSD1306 = 0,
	clockTIMER_SSD1306_2 = 0,
	clockTIMER_M90EX6 = 0, 								// set ALL to ZERO ir overlap
#endif
//	clockTIMER_DS2482,
	clockTIMER_NUM,										// last in list, define all required above here
} ;

enum {													// tickTIMERs
	tickTIMER_L2,										// track L2 disconnected time & occurrences
	tickTIMER_L3,										// track L3 disconnected time & occurrences
	tickTIMER_MQTT,
//	tickTIMER_DS2482,
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
