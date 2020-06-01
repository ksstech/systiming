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
 * systiming.h
 */

#include	"x_definitions.h"

#include	<stdbool.h>
#include	<stdint.h>

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// #################################################################################################

#define	systimerSCATTER							1			// enable scatter graphs support
#define	systimerSCATTER_GROUPS					10			// number of scatter graph groupings
#define	systimerSCATTER_HDR_SHOW				0			// 0=no scatter heading ie intelligent display

#if		(systimerSCATTER == 1)
	#define	IF_SYSTIMER_INIT(T, n, t, tag, ...)		if (T) vSysTimerInit(n, t, tag, ##__VA_ARGS__)
#else
	// Allows macro to take scatter parameters (hence avoid errors if used in macro definition)
	// but discard values passed
	#define	IF_SYSTIMER_INIT(T, n, t, tag, ...)		if (T) vSysTimerInit(n, t, tag)
#endif
#define	IF_SYSTIMER_START(T,y)					if (T) xSysTimerStart(y)
#define	IF_SYSTIMER_STOP(T,y)					if (T) xSysTimerStop(y)
#define	IF_SYSTIMER_RESET(T,y)					if (T) xSysTimerReset(y)

#define	IF_SYSTIMER_SHOW(T,m)					if (T) vSysTimerShow(m)
#define	IF_SYSTIMER_SHOW_NUM(T,n)				if (T) vSysTimerShow(1 << n)

// ################################# Process timer support #########################################

enum { systimerTICKS, systimerCLOCKS } ;

enum {
	systimerL2, 		systimerL3,						// track Lx disconnected time & occurrences
	systimerMQTT_RX,	systimerMQTT_TX,
//	systimerHTTP,
//	systimerFOTA,
//	systimerSLOG,
	systimerPCA9555,
	systimerDS248x1, systimerDS248x2, systimerDS248x3,
//	systimerDS18X20,
//	systimerM90EX6,
	systimerACT_S0, systimerACT_S1, systimerACT_S2, systimerACT_S3, systimerACT_SX,
//	systimerSSD1306, systimerSSD1306_2,
	systimerILI9341_1, systimerILI9341_2,
//	systimerTFTP,										// TFTP task execution timing...
	systimerMAX_NUM,									// last in list, define all required above here

// From here we list disabled timers to avoid compile errors
//	systimerMQTT_RX = 31,	systimerMQTT_TX = 31,
	systimerHTTP = 31,
	systimerFOTA = 31,
	systimerSLOG = 31,
//	systimerPCA9555 = 31,
//	systimerDS248x1 = 31, systimerDS248x2 = 31, systimerDS248x3 = 31,
	systimerDS18X20,
	systimerM90EX6 = 31,
//	systimerACT_S0 = 31, systimerACT_S1 = 31, systimerACT_S2 = 31, systimerACT_S3 = 31, systimerACT_SX = 31,
	systimerSSD1306 = 31, systimerSSD1306_2 = 31,
//	systimerILI9341_1 = 31, systimerili9341_2 = 31,
	systimerTFTP = 31,
} ;

// ######################################### Data structures #######################################

typedef struct __attribute__((packed)) systimer_s {
	const char * Tag ;
	uint64_t	Sum ;
	uint32_t	Last, Count, Min, Max ;
#if		(systimerSCATTER == 1)
	uint32_t	SGmin, SGmax ;
	uint32_t	Group[systimerSCATTER_GROUPS] ;
#endif
} systimer_t ;

#if		(systimerSCATTER == 1)
	DUMB_STATIC_ASSERT(sizeof(systimer_t) == sizeof(char*) + sizeof(uint64_t) + (sizeof(uint32_t) * (6 + systimerSCATTER_GROUPS))) ;
#else
	DUMB_STATIC_ASSERT(sizeof(systimer_t) == sizeof(char*) + sizeof(uint64_t) + (sizeof(uint32_t) * 4)) ;
#endif

// ######################################### Public variables ######################################


// ######################################### Public functions ######################################

void	vSysTimerResetCounters(uint8_t TimNum) ;
void	vSysTimerInit(uint8_t TimNum, bool Type, const char * Tag, ...) ;
void	vSysTimerResetCountersMask(uint32_t TimerMask) ;

uint32_t xSysTimerStart(uint8_t TimNum) ;
uint32_t xSysTimerStop(uint8_t TimNum) ;
uint32_t xSysTimerIsRunning(uint8_t TimNum) ;
bool	bSysTimerGetStatus(uint8_t TimNum, systimer_t *) ;

uint64_t xSysTimerGetElapsedClocks(uint8_t TimNum) ;
uint64_t xSysTimerGetElapsedMicros(uint8_t TimNum) ;
uint64_t xSysTimerGetElapsedMillis(uint8_t TimNum) ;
uint64_t xSysTimerGetElapsedSecs(uint8_t TimNum) ;

void	vSysTimerShow(uint32_t TimerMask) ;

uint32_t xClockDelayUsec(uint32_t uSec) ;
uint32_t xClockDelayMsec(uint32_t mSec) ;

void	vSysTimingTest(void) ;

#ifdef __cplusplus
}
#endif
