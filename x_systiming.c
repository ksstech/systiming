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

#define	debugFLAG					0x4000

#define	debugPARAM					(debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG & 0x8000)

// ################################# Code execution timer support ##################################

static systimer_t	STdata[systimerMAX_NUM] = { 0 } ;
static uint32_t		STstat = 0 ;
static uint32_t		STtype = 0 ;

#define	SYSTIMER_TYPE(x)	(STtype & (1UL << x))

/* Functions rely on the GET_CLOCK_COUNTER definition being present and correct in "hal_timer.h"
 * Maximum period that can be delayed or measured is UINT32_MAX clock cycles.
 * This translates to:
 * 		80Mhz		53.687,091 Sec
 * 		100Mhz		42.949,672 Sec
 * 		120Mhz		35.791,394 Sec
 * 		160Mhz		26.843,546 Sec
 * 		240Mhz		17.895,697 Sec
 *
 * If it is found that values radically jump around it is most likely related to the task
 * where measurements are started, stopped or reported from NOT RUNNING on a SPECIFIC core
 * Pin the task to a specific core and the problem should go away.
 */

/**
 * vSysTimerReset() -
 * @param TimerMask
 */
#if		(systimerSCATTER == 1)
void	vSysTimerReset(uint32_t TimerMask, bool Type, uint32_t Min, uint32_t Max) {
#else
void	vSysTimerReset(uint32_t TimerMask, bool Type) {
#endif
	IF_myASSERT(debugPARAM, Min < Max) ;
	uint32_t	mask = 0x00000001 ;
	systimer_t *pST	= STdata ;
	for (uint8_t TimNum = 0; TimNum < systimerMAX_NUM; ++TimNum, ++pST) {
		if (TimerMask & mask) {
			pST->Sum	= 0ULL ;
			pST->Last	= pST->Count = 0UL ;
			pST->Max	= UINT32_MIN ;
			pST->Min	= UINT32_MAX ;
			STstat		&= ~(1UL << TimNum) ;			// clear active status ie STOP
			if (Type) {
				STtype |= (1UL << TimNum) ;				// mark as CLOCK type
			} else {
				STtype &= ~(1UL << TimNum) ;			// mark as TICK type
			}
#if		(systimerSCATTER == 1)
			pST->SGmin	= Min ;
			pST->SGmax	= Max ;
			pST->SGfact	= (pST->SGmax - pST->SGmin) / (systimerSCATTER_GROUPS-2) ;
			memset(&pST->Group, 0, SIZEOF_MEMBER(systimer_t, Group)) ;
#endif
		}
		mask <<= 1 ;
	}
}

/**
 * xSysTimerStart() - start the specified timer
 * @param 		TimNum
 * @return		current timer value based on type (CLOCKs or TICKs)
 */
uint32_t xSysTimerStart(uint8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < systimerMAX_NUM) ;
	STstat	|= (1UL << TimNum) ;				// Mark as started & running
	return (STdata[TimNum].Last = SYSTIMER_TYPE(TimNum) ? GET_CLOCK_COUNTER() : xTaskGetTickCount()) ;
}

/**
 * xSysTimerStop() stop the specified timer and update the statistics
 * @param	TimNum
 * @return	Last measured interval based on type (CLOCKs or TICKs)
 */
uint32_t xSysTimerStop(uint8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < systimerMAX_NUM) ;
	uint32_t tNow	= SYSTIMER_TYPE(TimNum) ? GET_CLOCK_COUNTER() : xTaskGetTickCount() ;
	STstat			&= ~(1UL << TimNum) ;				// mark as stopped
	systimer_t *pST	= &STdata[TimNum] ;
	if (SYSTIMER_TYPE(TimNum)) {
		pST->Last	= tNow > pST->Last ? tNow - pST->Last : tNow + (0xFFFFFFFF - pST->Last) ;
	} else {
		pST->Last	= tNow - pST->Last ;				// very unlikely wrap
	}
	if (pST->Last < pST->Min) {
		pST->Min	=  pST->Last ;
	}
	if (pST->Last > pST->Max) {
		pST->Max	=  pST->Last ;
	}
	pST->Sum		+= pST->Last ;
	++pST->Count ;
#if		(systimerSCATTER == 1)
	if (pST->Last <= pST->SGmin) {
		++pST->Group[0] ;
	} else if (pST->Last >= pST->SGmax) {
		++pST->Group[systimerSCATTER_GROUPS-1] ;
	} else {
		++pST->Group[((pST->Last - pST->SGmin) / pST->SGfact) + 1] ;
	}
#endif
	return pST->Last ;
}

/**
 * xSysTimerIsRunning() -  if timer is running, return value else 0
 * @param	TimNum
 * @return	0 if not running
 * 			current elapsed timer value based on type (CLOCKs or TICKSs)
 */
uint32_t xSysTimerIsRunning(uint8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < systimerMAX_NUM) ;
	uint32_t	tNow = SYSTIMER_TYPE(TimNum) ? GET_CLOCK_COUNTER() : xTaskGetTickCount() ;
	if (SYSTIMER_TYPE(TimNum)) {
		systimer_t * pST = &STdata[TimNum] ;
		if (tNow > pST->Last) {							// from here outside of running timer context
			tNow -= pST->Last ;							// most likely NO wrap
		} else {
			tNow += (0xFFFFFFFF - pST->Last) ;			// definitely wrapped
		}
	} else {
		tNow = 0 ;
	}
	return tNow ;
}

/**
 * bSysTimerGetStatus() - return the current timer values and type
 * @param	TimNum
 * @param	pST
 * @return	Type being 0=TICK 1=CLOCK
 */
bool	bSysTimerGetStatus(uint8_t TimNum, systimer_t * pST) {
	IF_myASSERT(debugPARAM, TimNum < systimerMAX_NUM && INRANGE_SRAM(pST)) ;
	memcpy(pST, &STdata[TimNum], sizeof(systimer_t)) ;
	return SYSTIMER_TYPE(TimNum) ? 1 : 0 ;				// return the Type (0=TICK 1=CLOCK)
}

uint64_t xSysTimerGetElapsedClocks(uint8_t TimNum) {
	IF_myASSERT(debugPARAM, (TimNum < systimerMAX_NUM) && SYSTIMER_TYPE(TimNum)) ;
	return STdata[TimNum].Sum ;
}

uint64_t xSysTimerGetElapsedMicros(uint8_t TimNum) {
	IF_myASSERT(debugPARAM, (TimNum < systimerMAX_NUM) && SYSTIMER_TYPE(TimNum)) ;
	return myCLOCKS_TO_US(STdata[TimNum].Sum, uint64_t) ;
}

uint64_t xSysTimerGetElapsedMillis(uint8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < systimerMAX_NUM) ;
	return SYSTIMER_TYPE(TimNum) ? myCLOCKS_TO_MS(STdata[TimNum].Sum, uint64_t) : myTICKS_TO_MS(STdata[TimNum].Sum, uint64_t) ;
}

uint64_t xSysTimerGetElapsedSecs(uint8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < systimerMAX_NUM) ;
	return SYSTIMER_TYPE(TimNum) ? myCLOCKS_TO_SEC(STdata[TimNum].Sum, uint64_t) : myTICKS_TO_MS(STdata[TimNum].Sum, uint64_t) ;
}

#define	systimerHDR_TICKS	"| # | Count   | Last mSec| Min mSec | Max mSec | Avg mSec | Total mSec |"
#define	systimerHDR_CLOCKS	"| # | Count   | Last uSec  | Min uSec   | Max uSec   | Avg uSec   | Total uSec   |"	\
							" Last Clocks  | Min Clocks   | Max Clocks   | Avg Clocks   | Total Clocks   |"

void	vSysTimerShowHeading(int32_t Handle, const char * Header, systimer_t * pST) {
	xdprintf(Handle, Header) ;
#if		(systimerSCATTER == 1) && (systimerSHATTER_HDR_SHOW == 1)
	for (int32_t Idx = 0; Idx < systimerSCATTER_GROUPS; ++Idx) {
		xdprintf(Handle, "%-6u+|", Idx == 0 ? 0 :
									Idx == 1 ? pST->SGmin + 1 :
									Idx == systimerSCATTER_GROUPS-1 ? pST->SGmax :
									pST->SGmin + (pST->SGfact * (Idx-1)) + 1) ;
	}
#endif
	xdprintf(Handle, "\n") ;
}

void	vSysTimerShowScatter(int32_t Handle, systimer_t * pST) {
#if (systimerSHATTER_HDR_SHOW == 1)
	for (int32_t Idx = 0; Idx < systimerSCATTER_GROUPS; xdprintf(Handle, "%7u|", pST->Group[Idx++])) ;
#else
	for (int32_t Idx = 0; Idx < systimerSCATTER_GROUPS; ++Idx) {
		if (pST->Group[Idx]) {
#if 0
			uint32_t Val, Div, Sep ;
			Val	= Idx == 0 ? 0 : pST->SGmin + ((Idx - 1) * pST->SGfact) + 1 ;
			Div	= Val > (10*MILLION) ? MILLION : Val > (10*THOUSAND) ? THOUSAND : 1 ;
			Sep	= Val > (10*MILLION) ? CHR_M : Val > (10*THOUSAND) ? CHR_K : CHR_NUL ;
			xdprintf(Handle, "  %u%c~", Val / Div, Sep) ;

			Val	= pST->SGmin + (Idx * pST->SGfact) ;
			Div	= Val > (10*MILLION) ? MILLION : Val > (10*THOUSAND) ? THOUSAND : 1 ;
			Sep	= Val > (10*MILLION) ? CHR_M : Val > (10*THOUSAND) ? CHR_K : CHR_NUL ;
			xdprintf(Handle, "%u%c=%u", Val / Div, Sep, pST->Group[Idx]) ;
#else
			xdprintf(Handle, "  %#u~%#u=%u",
				Idx == 0 ? 0 : pST->SGmin + ((Idx - 1) * pST->SGfact) + 1,
				pST->SGmin + (Idx * pST->SGfact),
				pST->Group[Idx]) ;
#endif

		}
	}
#endif
}

/**
 * vSysTimerShow(tMask) - display the current value(s) of the specified timer(s)
 * @brief	MUST do a SysTimerStop() before calling to freeze accurate value in array
 * @param	tMask 8bit bitmapped flag to select timer(s) to display
 * @return	none
 */
void	vSysTimerShow(int32_t Handle, uint32_t TimerMask) {
#if 	(systimerSHATTER_HDR_SHOW == 1)
	uint32_t	Mask, TimNum ;
	systimer_t * pST ;
	for (TimNum = 0, pST = STdata, Mask = 0x00000001; TimNum < systimerMAX_NUM; ++TimNum, ++pST) {
		if ((TimerMask & Mask) && pST->Count) {
			vSysTimerShowHeading(Handle, SYSTIMER_TYPE(TimNum) ? systimerHDR_CLOCKS : systimerHDR_TICKS, pST) ;
			if (SYSTIMER_TYPE(TimNum)) {
				xdprintf(Handle, "|%2d |%'9u|%'12llu|%'14llu|",
					TimNum,
					pST->Count,
					myCLOCKS_TO_US(pST->Sum, uint64_t) / (uint64_t) pST->Count,
					myCLOCKS_TO_US(pST->Sum, uint64_t)) ;
				xdprintf(Handle, "%'12u|%'12u|%'12u|%'14u|",
					myCLOCKS_TO_US(pST->Last, uint32_t),
					myCLOCKS_TO_US(pST->Min, uint32_t),
					myCLOCKS_TO_US(pST->Max, uint32_t),
					pST->Sum / (uint64_t) pST->Count) ;
				xdprintf(Handle, "%'16u|%'14u|%'14u|%'14u|",
					pST->Sum, pST->Last, pST->Min, pST->Max) ;
			} else {
				xdprintf(Handle, "|%2d |%'9u|%'10llu|%'12llu|",
					TimNum,
					pST->Count,
					myTICKS_TO_MS(pST->Sum, uint64_t) / (uint64_t) pST->Count,
					myTICKS_TO_MS(pST->Sum, uint64_t)) ;
				xdprintf(Handle, "%'10u|%'10u|%'10u|",
					myTICKS_TO_MS(pST->Last, uint32_t),
					pST->Min,
					pST->Max) ;
			}
#if		(systimerSCATTER == 1)
			vSysTimerShowScatter(Handle, pST) ;
#endif
			xdprintf(Handle, "\n\n") ;
		}
		Mask <<= 1 ;
	}
#else
	uint32_t	Mask, TimNum ;
	systimer_t * pST ;
	bool	HdrDone ;
	for (TimNum = 0, pST = STdata, Mask = 0x00000001, HdrDone = 0; TimNum < systimerMAX_NUM; ++TimNum, ++pST) {
		if ((TimerMask & Mask) && pST->Count) {
			if (SYSTIMER_TYPE(TimNum)) {
				if (HdrDone == 0) { vSysTimerShowHeading(Handle, systimerHDR_CLOCKS, pST) ; HdrDone = 1 ; }
//				xdprintf(Handle, "|%2d |%'9u|%'12u|%'12u|",
				xdprintf(Handle, "|%2d |%'9u|%'#12u|%'#12u|",
					TimNum,
					pST->Count,
					myCLOCKS_TO_US(pST->Last, uint32_t),
					myCLOCKS_TO_US(pST->Min, uint32_t)) ;
//				xdprintf(Handle, "%'12u|%'12llu|%'14llu|%'14u|",
				xdprintf(Handle, "%#'12u|%#'12llu|%#'14llu|%#'14u|",
					myCLOCKS_TO_US(pST->Max, uint32_t),
					myCLOCKS_TO_US(pST->Sum, uint64_t) / (uint64_t) pST->Count,
					myCLOCKS_TO_US(pST->Sum, uint64_t),
					pST->Last) ;
//				xdprintf(Handle, "%'14u|%'14u|%'14llu|%'16llu|",
				xdprintf(Handle, "%#'14u|%#'14u|%#'14llu|%#'16llu|",
					pST->Min,
					pST->Max,
					pST->Sum / (uint64_t) pST->Count,
					pST->Sum) ;
#if		(systimerSCATTER == 1)
				vSysTimerShowScatter(Handle, pST) ;
#endif
				xdprintf(Handle, "\n") ;
			}
		}
		Mask <<= 1 ;
	}

	for (TimNum = 0, pST = STdata, Mask = 0x00000001, HdrDone = 0; TimNum < systimerMAX_NUM; ++TimNum, ++pST) {
		if ((TimerMask & Mask) && pST->Count) {
			if (!SYSTIMER_TYPE(TimNum)) {
				if (HdrDone == 0) { vSysTimerShowHeading(Handle, systimerHDR_TICKS, pST) ; HdrDone = 1 ; }
				xdprintf(Handle, "|%2d |%'9u|%'10u|%'10u|",
					TimNum,
					pST->Count,
					myTICKS_TO_MS(pST->Last, uint32_t),
					myTICKS_TO_MS(pST->Min, uint32_t)) ;
				xdprintf(Handle, "%'10u|%'10llu|%'12llu|",
					myTICKS_TO_MS(pST->Max, uint32_t),
					myTICKS_TO_MS(pST->Sum, uint64_t) / (uint64_t) pST->Count,
					myTICKS_TO_MS(pST->Sum, uint64_t)) ;
#if		(systimerSCATTER == 1)
				vSysTimerShowScatter(Handle, pST) ;
#endif
				xdprintf(Handle, "\n") ;
			}
		}
		Mask <<= 1 ;
	}
	xdprintf(Handle, "\n") ;
#endif
}

// ################################## MCU Clock cycle delay support ################################

/**
 * vClockDelayUsec() - delay (not yielding) program execution for a specified number of uSecs
 * @param	Number of uSecs to delay
 * @return	Clock counter at the end
 */
uint32_t xClockDelayUsec(uint32_t uSec) {
	IF_myASSERT(debugPARAM, uSec < (UINT32_MAX / configCLOCKS_PER_USEC)) ;
	uint32_t ClockEnd	= GET_CLOCK_COUNTER() + halUS_TO_CLOCKS(uSec) ;
	while ((ClockEnd - GET_CLOCK_COUNTER()) > configCLOCKS_PER_USEC ) ;
	return ClockEnd ;
}

/**
 * xClockDelayMsec() - delay (not yielding) program execution for a specified number of mSecs
 * @param	Number of mSecs to delay
 * #return	Clock counter at the end
 */
uint32_t xClockDelayMsec(uint32_t mSec) {
	IF_myASSERT(debugPARAM, mSec < (UINT32_MAX / configCLOCKS_PER_MSEC)) ;
	return xClockDelayUsec(mSec * MICROS_IN_MILLISEC) ;
}

// ##################################### functional tests ##########################################

void	vSysTimingTest(void) {
	uint32_t	uCount, uSecs ;
	// Test the uSec delays
	uCount	= GET_CLOCK_COUNTER() ;
	uSecs	= xClockDelayUsec(100) ;
	SL_DBG("Delay=%'u uS\r\n", (uSecs - uCount) / configCLOCKS_PER_USEC) ;

	uCount	= GET_CLOCK_COUNTER() ;
	uSecs	= xClockDelayUsec(1000) ;
	SL_DBG("Delay=%'u uS\r\n", (uSecs - uCount) / configCLOCKS_PER_USEC) ;

	uCount	= GET_CLOCK_COUNTER() ;
	uSecs	= xClockDelayUsec(10000) ;
	SL_DBG("Delay=%'u uS\r\n", (uSecs - uCount) / configCLOCKS_PER_USEC) ;

	// Test the mSec timers
	vSysTimerReset(0xFFFFFFFF, 0, 5, 20) ;
	for (uCount = 0; uCount < systimerMAX_NUM; uCount++) { xClockDelayMsec(2) ; xSysTimerStart(uCount) ; }
	for (uCount = 0; uCount < systimerMAX_NUM; uCount++) { xClockDelayMsec(4) ; xSysTimerStop(uCount) ; }
	vSysTimerShow(1, 0xFFFF) ;

	// Test the Clock Cycle timers
	vSysTimerReset(0xFFFFFFFF, 1, myMS_TO_CLOCKS(1), myMS_TO_CLOCKS(20)) ;
	for (uCount = 0; uCount < systimerMAX_NUM; uCount++) { xClockDelayMsec(2) ;xSysTimerStart(uCount) ; }
	for (uCount = 0; uCount < systimerMAX_NUM; uCount++) { xClockDelayMsec(4) ; xSysTimerStop(uCount) ; }
	vSysTimerShow(1, 0xFFFF) ;
}
