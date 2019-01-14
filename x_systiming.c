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
 * 	x_systiming.c
 */

#include	"hal_config.h"
#include	"hal_timer.h"

#include	"x_debug.h"
#include	"x_syslog.h"
#include	"x_systiming.h"

#include	"FreeRTOS_Support.h"
#include	<string.h>

#define	debugFLAG					0x0000

#define	debugPARAM					(debugFLAG & 0x0001)

// ################################# Code execution timer support ##################################

ticktime_t	TickTimer[tickTIMER_NUM] = { 0 } ;
uint32_t	TickStatus = 0 ;

/**
 * vTickTimerReset() -
 * @param TimerMask
 */
void	vTickTimerReset(uint32_t TimerMask) {
	uint32_t	mask = 0x0001 ;
	for (uint8_t TimNum = 0; TimNum < tickTIMER_NUM; TimNum++) {
		if (TimerMask & mask) {
			ticktime_t * pTT = &TickTimer[TimNum] ;
			pTT->Sum = pTT->Last = pTT->Count = 0UL ;
#if		(buildTICKTIMER_STATS == 1)
			pTT->Max = 0UL ;
			pTT->Min = UINT32_MAX ;
#endif
			TickStatus	&= ~(1UL << TimNum) ;
		}
		mask <<= 1 ;
	}
}

/*
 * xTickTimerStart(tNumber) - start the specified Tick timer
 * \param	tNumber - number of the time to start
 * \return	the current value of the Tick timer
 */
uint32_t xTickTimerStart(uint8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < tickTIMER_NUM) ;					// function overhead effectively NIL now
	TickStatus	|= (1UL << TimNum) ;
	return (TickTimer[TimNum].Last = xTaskGetTickCount()) ;
}

/*
 * xTickTimerStop() stop the specified Tick timer
 * \param	tNumber - number of timer to stop
 * \return	the number of clock cycles elapsed since the timer was started
 */
uint32_t xTickTimerStop(uint8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < tickTIMER_NUM) ;
	ticktime_t * pTT = &TickTimer[TimNum] ;
	pTT->Last	= xTaskGetTickCount() - pTT->Last ;
#if		(buildTICKTIMER_STATS == 1)
	pTT->Min	= pTT->Last < pTT->Min ? pTT->Last : pTT->Min ;
	pTT->Max	= pTT->Last > pTT->Max ? pTT->Last : pTT->Max ;
#endif
	pTT->Sum	+= pTT->Last ;
	++pTT->Count ;
	TickStatus	&= ~(1UL << TimNum) ;
	return pTT->Last ;
}

/**
 * xTickTimerIsRunning()
 *
 * @return	number of ticks since started or 0 if not running
 */
uint32_t xTickTimerIsRunning(uint8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < tickTIMER_NUM) ;
	return (TickStatus & (1UL << TimNum)) ? (xTaskGetTickCount() - TickTimer[TimNum].Last) : 0 ;
}

uint32_t xTickTimerGetElapsedMillis(uint8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < clockTIMER_NUM) ;
	return (TickTimer[TimNum].Sum * MILLIS_IN_SECOND) / configTICK_RATE_HZ ;
}

uint32_t xTickTimerGetElapsedSecs(uint8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < clockTIMER_NUM) ;
	return TickTimer[TimNum].Sum / configTICK_RATE_HZ ;
}

void	vTickTimerGetStatus(uint8_t TimNum, ticktime_t * pTT) {
	IF_myASSERT(debugPARAM, TimNum < clockTIMER_NUM && INRANGE_SRAM(pTT)) ;
	memcpy(pTT, &TickTimer[TimNum], sizeof(ticktime_t)) ;
}

/*
 * vTickTimerShow(tMask) - display the current value(s) of the specified timer(s)
 * \brief	MUST do a TickTimerStop before calling to freeze accurate value in array
 * \param	tMask 8bit bitmapped flag to select timer(s) to display
 * \return	none
 */
void	vTickTimerShow(int32_t Handle, uint32_t TimerMask) {
	uint32_t	mask = 1, HdrDone = 0 ;
	for (int32_t TimNum = 0; TimNum < tickTIMER_NUM; TimNum++) {
		ticktime_t * pTT = &TickTimer[TimNum] ;
		if ((TimerMask & mask) && pTT->Count) {
			if (HdrDone == 0) {
#if		(buildTICKTIMER_STATS == 1)
				xdprintf(Handle, "| # |  Counts  | Total mSec | Avg mSec | Last mSec| Min mSec | Max mSec |\n") ;
# else
				xdprintf(Handle, "| # |  Counts  | Total mSec | Avg mSec | Last mSec|\n") ;
#endif
				HdrDone = 1 ;
			}
#if		(buildTICKTIMER_STATS == 1)
			xdprintf(Handle, "|%2d |%'10u|%'12u|%'10u|%'10u|%'10u|%'10u|\n", TimNum, pTT->Count,
					(pTT->Sum * MILLIS_IN_SECOND) / configTICK_RATE_HZ,
					((pTT->Sum / pTT->Count) * MILLIS_IN_SECOND) / configTICK_RATE_HZ,
					(pTT->Last * MILLIS_IN_SECOND) / configTICK_RATE_HZ,
					pTT->Min, pTT->Max) ;
#else
			xdprintf(Handle, "|%2d |%'10u|%'12u|%'10u|%'10u|\n", TimNum, pTT->Count,
					(pTT->Sum * MILLIS_IN_SECOND) / configTICK_RATE_HZ,
					((pTT->Sum / pTT->Count) * MILLIS_IN_SECOND) / configTICK_RATE_HZ,
					(pTT->Last * MILLIS_IN_SECOND) / configTICK_RATE_HZ) ;
#endif
		}
		mask <<= 1 ;
	}
}

// ################################## MCU Clock cycle timer support ################################

/* Functions rely on the GET_CLOCK_COUNTER definition being present and correct in "hal_timer.h"
 * Maximum period that can be delayed or measured is UINT32_MAX clock cycles.
 * This translates to:
 * 		80Mhz		53.687,091 Sec
 * 		100Mhz		42.949,672 Sec
 * 		120Mhz		35.791,394 Sec
 * 		160Mhz		26.843,546 Sec
 * 		240Mhz		17.895,697 Sec
 */

clocktime_t	ClockTimer[clockTIMER_NUM] = { 0 } ;
uint32_t	ClockStatus = 0 ;

/**
 * vClockTimerReset() -
 * @param TimerMask
 */
void	vClockTimerReset(uint8_t TimerMask) {
	uint32_t	mask = 0x0001 ;
	for (uint8_t TimNum = 0; TimNum < clockTIMER_NUM; TimNum++) {
		if (TimerMask & mask) {
			clocktime_t * pCT = &ClockTimer[TimNum] ;
			pCT->Sum	= 0ULL ;
			pCT->Last	= pCT->Count = 0UL ;
#if		(buildTICKTIMER_STATS == 1)
			pCT->Max	= UINT32_MIN ;
			pCT->Min	= UINT32_MAX ;
#endif
			ClockStatus	&= ~(1UL << TimNum) ;
		}
		mask <<= 1 ;
	}
}

/*
 * xClockTimerStart() - start the specified clock cycle timer
 * \param	tNumber - number of the timer to start
 * \return	the current value of the selected clock cycle timer
 */
uint32_t xClockTimerStart(uint8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < clockTIMER_NUM) ;	// excludes overhead of assert()
	ClockStatus	|= (1UL << TimNum) ;					// and the status mask update  from timer context
	return ClockTimer[TimNum].Last = GET_CLOCK_COUNTER ;
}

/*
 * xClockTimerStop() stop the specified uSec timer
 * \param	tNumber - number of the clock timer to stop
 * \return	the number of clock cycles elapsed since the timer was started
 */
uint32_t xClockTimerStop(uint8_t TimNum) {
	uint32_t tNow = GET_CLOCK_COUNTER ;					// immediately read the clock counter.
	IF_myASSERT(debugPARAM, TimNum < clockTIMER_NUM) ;	// function overhead effectively NIL now
	clocktime_t * pCT = &ClockTimer[TimNum] ;
	if (tNow > pCT->Last) {								// from here outside of running timer context
		pCT->Last = tNow - pCT->Last ;					// assume no wrap
	} else {
		pCT->Last = tNow + (0xFFFFFFFF - pCT->Last) ;	// definitely wrapped
	}
#if		(buildTICKTIMER_STATS == 1)
	if (pCT->Last < pCT->Min)		pCT->Min =  pCT->Last ;
	if (pCT->Last > pCT->Max)		pCT->Max =  pCT->Last ;
#endif
	pCT->Sum	+= pCT->Last ;
	++pCT->Count ;
	ClockStatus	&= ~(1UL << TimNum) ;
	return pCT->Last ;
}

/**
 * xClockTimerIsRunning()
 *
 * @return	number of clocks since started or 0 if not running
 */
uint32_t xClockTimerIsRunning(uint8_t TimNum) {
	uint32_t tNow = GET_CLOCK_COUNTER ;					// immediately read the clock counter.
	IF_myASSERT(debugPARAM, TimNum < tickTIMER_NUM) ;
	if (ClockStatus & (1UL << TimNum)) {
		clocktime_t * pCT = &ClockTimer[TimNum] ;
		if (tNow > pCT->Last) {							// from here outside of running timer context
			return tNow - pCT->Last ;					// assume no wrap
		} else {
			return tNow + (0xFFFFFFFF - pCT->Last) ;	// definitely wrapped
		}
	}
	return 0 ;
}

uint64_t xClockTimerGetElapsedClocks(uint8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < clockTIMER_NUM) ;
	return ClockTimer[TimNum].Sum ;
}

uint64_t xClockTimerGetElapsedMicros(uint8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < clockTIMER_NUM) ;
	return ClockTimer[TimNum].Sum / (uint64_t) configCLOCKS_PER_USEC ;
}

uint64_t xClockTimerGetElapsedMillis(uint8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < clockTIMER_NUM) ;
	return ClockTimer[TimNum].Sum / (uint64_t) configCLOCKS_PER_MSEC ;
}

uint64_t xClockTimerGetElapsedSecs(uint8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < clockTIMER_NUM) ;
	return ClockTimer[TimNum].Sum / (uint64_t) configCLOCKS_PER_SEC ;
}

void	vClockTimerGetStatus(uint8_t TimNum, clocktime_t * pCT) {
	IF_myASSERT(debugPARAM, TimNum < clockTIMER_NUM && INRANGE_SRAM(pCT)) ;
	memcpy(pCT, &ClockTimer[TimNum], sizeof(clocktime_t)) ;
}

/*
 * vClockTimerShow() display the current value(s) of the selected/specified timer(s)
 * \brief	MUST do a uSecTimerStop before calling to freeze accurate value in array
 * \param	8bit wide (in LSB) bitmapped flag to select timer(s)s to display
 * \return	none
 */
//#define HDR1	"| # |  Counts |  Total uSec  |  Total Clocks  |    Avg uSec  |    Avg Clocks  |    Last uSec |   Last Clocks  |"
#define HDR1	"| # | Count   | Total uSec   | Total Clocks   | Avg uSec     | Avg Clocks     | Last uSec    | Last Clocks    |"
#if		(buildTICKTIMER_STATS == 1)
//	#define HDR2	"   Min Clocks |    Max Clocks  |\n"
	#define HDR2	" Min Clocks   | Max Clocks     |\n"
#else
	#define	HDR2	"\n"
#endif
void	vClockTimerShow(int32_t Handle, uint32_t TimerMask) {
	uint32_t	mask = 0x0001 ;
	uint32_t	HdrDone = 0 ;
	for (int32_t TimNum = 0; TimNum < clockTIMER_NUM; TimNum++) {
		clocktime_t * pCT = &ClockTimer[TimNum] ;
		if ((TimerMask & mask) && pCT->Count) {
			if (HdrDone == 0) {
				xdprintf(Handle, HDR1 HDR2) ;
				HdrDone = 1 ;
			}
			xdprintf(Handle, "|%2d |%'9u|%'14llu|%'16llu|",
				TimNum, pCT->Count,
				pCT->Sum / configCLOCKS_PER_USEC, pCT->Sum) ;
			xdprintf(Handle, "%'14llu|%'16llu|%'14u|%'16u|",
				(pCT->Sum / pCT->Count) / configCLOCKS_PER_USEC, pCT->Sum / pCT->Count,
				pCT->Last / configCLOCKS_PER_USEC, pCT->Last) ;
#if		(buildTICKTIMER_STATS == 1)
			xdprintf(Handle, "%'14u|%'16u|\n", pCT->Min, pCT->Max) ;
#else
			xdprintf(Handle, "\n") ;
#endif
		}
		mask <<= 1 ;
	}
}

/*
 * vClockDelayUsec() - delay (not yielding) program execution for a specified number of uSecs
 * \param	Number of uSecs to delay
 * \return	Clock counter at the end
 */
uint32_t xClockDelayUsec(uint32_t uSec) {
	IF_myASSERT(debugPARAM, uSec < (UINT32_MAX / configCLOCKS_PER_USEC)) ;
	uint32_t ClockEnd	= GET_CLOCK_COUNTER + halUS_TO_CLOCKS(uSec) ;
	while ((ClockEnd - GET_CLOCK_COUNTER) > configCLOCKS_PER_USEC ) ;
	return ClockEnd ;
}

/*
 * xClockDelayMsec() - delay (not yielding) program execution for a specified number of mSecs
 * \param	Number of mSecs to delay
 * \return	Clock counter at the end
 */
uint32_t xClockDelayMsec(uint32_t mSec) {
	IF_myASSERT(debugPARAM, mSec < (UINT32_MAX / configCLOCKS_PER_MSEC)) ;
	return xClockDelayUsec(mSec * MICROS_IN_MILLISEC) ;
}

// ##################################### functional tests ##########################################

void	vSysTimingTest(void) {
	uint32_t	uCount, uSecs ;
// Test the uSec delays
	uCount	= GET_CLOCK_COUNTER ;
	uSecs	= xClockDelayUsec(100) ;
	SL_DBG("Delay=%'u uS\r\n", (uSecs - uCount) / configCLOCKS_PER_USEC) ;

	uCount	= GET_CLOCK_COUNTER ;
	uSecs	= xClockDelayUsec(1000) ;
	SL_DBG("Delay=%'u uS\r\n", (uSecs - uCount) / configCLOCKS_PER_USEC) ;

	uCount	= GET_CLOCK_COUNTER ;
	uSecs	= xClockDelayUsec(10000) ;
	SL_DBG("Delay=%'u uS\r\n", (uSecs - uCount) / configCLOCKS_PER_USEC) ;

// Test the mSec timers
	for (uint8_t uCount = 0; uCount < tickTIMER_NUM; uCount++) {
		xClockDelayMsec(2) ;
		xTickTimerStart(uCount) ;
	}
	for (uint8_t uCount = 0; uCount < tickTIMER_NUM; uCount++) {
		xClockDelayMsec(4) ;
		xTickTimerStop(uCount) ;
	}
	vTickTimerShow(1, 0xFFFF) ;

// Test the Clock Cycle timers
	for (uint8_t uCount = 0; uCount < clockTIMER_NUM; uCount++) {
		xClockDelayMsec(2) ;
		xClockTimerStart(uCount) ;
	}
	for (uint8_t uCount = 0; uCount < clockTIMER_NUM; uCount++) {
		xClockDelayMsec(4) ;
		xClockTimerStop(uCount) ;
	}
	vClockTimerShow(1, 0xFFFF) ;
}
