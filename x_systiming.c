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

//temporary
#include	"esp_panic.h"
#define	debugFLAG					0xC000

#define	debugPARAM					(debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG & 0x8000)

// ################################# Code execution timer support ##################################

#define	SYSTIMER_TYPE(x)	(STtype & (1UL << x))

static uint32_t		STstat = 0 ;
static uint32_t		STtype = 0 ;
static systimer_t	STdata[systimerMAX_NUM] = { 0 } ;

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
void	vSysTimerReset(uint32_t TimerMask, bool Type, const char * Tag, ...) {
	uint32_t	mask = 0x00000001 ;
	systimer_t *pST	= STdata ;
	for (uint8_t TimNum = 0; TimNum < systimerMAX_NUM; ++TimNum, ++pST) {
		if (TimerMask & mask) {
//			if (TimNum == systimerMQTT_TX) 	{ esp_clear_watchpoint(0) ; }
			memset(pST, 0, sizeof(systimer_t)) ;
			pST->Tag	= Tag ;
//			if (TimNum == systimerMQTT_TX)	{ esp_set_watchpoint(0, &pST->Tag, sizeof(const char *), ESP_WATCHPOINT_STORE) ; }
			pST->Min	= UINT32_MAX ;
			STstat		&= ~(1UL << TimNum) ;			// clear active status ie STOP
			if (Type) {
				STtype |= (1UL << TimNum) ;				// mark as CLOCK type
			} else {
				STtype &= ~(1UL << TimNum) ;			// mark as TICK type
			}
#if		(systimerSCATTER == 1)
		    va_list vaList ;
		    va_start(vaList, Tag) ;
			pST->SGmin	= va_arg(vaList, uint32_t) ;	// Min
			pST->SGmax	= va_arg(vaList, uint32_t) ;	// Max ;
			IF_myASSERT(debugPARAM, pST->SGmin < pST->SGmax) ;
			va_end(vaList) ;
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
	if (SYSTIMER_TYPE(TimNum)) {						// CLOCK type ?
		pST->Last	= tNow > pST->Last ? tNow - pST->Last : tNow + (0xFFFFFFFF - pST->Last) ;
	} else {											// TICK type...
		pST->Last	= tNow - pST->Last ;				// very unlikely wrap
	}
	pST->Sum		+= pST->Last ;
	pST->Count++ ;
	// update Min & Max if required
	if (pST->Min > pST->Last) {
		pST->Min	=  pST->Last ;
	}
	if (pST->Max < pST->Last) {
		pST->Max	=  pST->Last ;
	}
#if		(systimerSCATTER == 1)
	int32_t Idx ;
	if (pST->Last <= pST->SGmin) {
		Idx = 0 ;
	} else if (pST->Last >= pST->SGmax) {
		Idx = systimerSCATTER_GROUPS-1 ;
	} else {
		Idx = 1 + ((pST->Last - pST->SGmin) * (systimerSCATTER_GROUPS-2) ) / (pST->SGmax - pST->SGmin) ;
	}
	IF_CPRINT(debugRESULT && OUTSIDE(0, Idx, systimerSCATTER_GROUPS-1, int32_t), "lo=%u hi=%u last=%u idx=%d\n", pST->SGmin, pST->SGmax, pST->Last, Idx) ;
	IF_myASSERT(debugRESULT, INRANGE(0, Idx, systimerSCATTER_GROUPS-1, int32_t)) ;
	++pST->Group[Idx] ;
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

#if 	(systimerSCATTER_HDR_SHOW == 1)
	#define	systimerHDR_TICKS	"| # |TickTMR | Count   | Last mSec| Min mSec | Max mSec | Avg mSec | Total mSec |"
	#define	systimerHDR_CLOCKS	"| # |ClockTMR| Count   | Last uSec  | Min uSec   | Max uSec   | Avg uSec   | Total uSec   |"	\
														" Last Clocks  | Min Clocks   | Max Clocks   | Avg Clocks   | Total Clocks   |"

void	vSysTimerShowHeading(const char * Header, systimer_t * pST) {
	xprintf(Header) ;
#if		(systimerSCATTER == 1)
	for (int32_t Idx = 0; Idx < systimerSCATTER_GROUPS; ++Idx) {
		xprintf("%-6u+|", Idx == 0 ? 0 :
									Idx == 1 ? pST->SGmin + 1 :
									Idx == systimerSCATTER_GROUPS-1 ? pST->SGmax :
									pST->SGmin + (pST->SGfact * (Idx-1)) + 1) ;
	}
#endif
	xprintf("\n") ;
}

void	vSysTimerShowScatter(systimer_t * pST) {
	for (int32_t Idx = 0; Idx < systimerSCATTER_GROUPS; xprintf("%7u|", pST->Group[Idx++])) ;
}

/**
 * vSysTimerShow(tMask) - display the current value(s) of the specified timer(s)
 * @brief	MUST do a SysTimerStop() before calling to freeze accurate value in array
 * @param	tMask 8bit bitmapped flag to select timer(s) to display
 * @return	none
 */
void	vSysTimerShow(uint32_t TimerMask) {
	uint32_t	Mask, TimNum ;
	systimer_t * pST ;
	for (TimNum = 0, pST = STdata, Mask = 0x00000001; TimNum < systimerMAX_NUM; ++TimNum, ++pST) {
		if ((TimerMask & Mask) && pST->Count) {
			vSysTimerShowHeading(Handle, SYSTIMER_TYPE(TimNum) ? systimerHDR_CLOCKS : systimerHDR_TICKS, pST) ;
			if (SYSTIMER_TYPE(TimNum)) {
				xprintf("|%2d |%8s|%'9u|%'12llu|%'14llu|",
					TimNum,
					pST->Tag,
					pST->Count,
					myCLOCKS_TO_US(pST->Sum, uint64_t) / (uint64_t) pST->Count,
					myCLOCKS_TO_US(pST->Sum, uint64_t)) ;
				xprintf("%'12u|%'12u|%'12u|%'14u|",
					myCLOCKS_TO_US(pST->Last, uint32_t),
					myCLOCKS_TO_US(pST->Min, uint32_t),
					myCLOCKS_TO_US(pST->Max, uint32_t),
					pST->Sum / (uint64_t) pST->Count) ;
				xprintf("%'16u|%'14u|%'14u|%'14u|",
					pST->Sum, pST->Last, pST->Min, pST->Max) ;
			} else {
				xprintf("|%2d |%8s|%'9u|%'10llu|%'12llu|",
					TimNum,
					pST->Tag,
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
}

#else

#define	systimerHDR_TICKS	"| # |TickTMR | Count |Last mS|Min mS |Max mS |Avg mS |Sum mS |\n"
#define	systimerHDR_CLOCKS	"| # |ClockTMR| Count |Last uS|Min uS |Max uS |Avg uS |Sum uS |"	\
												  "LastClk|Min Clk|Max Clk|Avg Clk|Sum Clk|\n"

void	vSysTimerShowScatter(systimer_t * pST) {
	uint32_t Rlo, Rhi ;
	for (int32_t Idx = 0; Idx < systimerSCATTER_GROUPS; ++Idx) {
		if (pST->Group[Idx]) {
			if (Idx == 0) {
				Rlo = UINT32_MIN ;
				Rhi = pST->SGmin ;
			} else if (Idx == (systimerSCATTER_GROUPS-1)) {
				Rlo = pST->SGmax ;
				Rhi = UINT32_MAX ;
			} else {
				Rlo	= (Idx - 1) * (pST->SGmax - pST->SGmin) / (systimerSCATTER_GROUPS-2) + pST->SGmin ;
				Rhi = Rlo + (pST->SGmax - pST->SGmin) / (systimerSCATTER_GROUPS-2) ;
			}
			xprintf("  #%d:%'#u->%'#u=%'#u", Idx, Rlo, Rhi, pST->Group[Idx]) ;
		}
	}
}

/**
 * vSysTimerShow(tMask) - display the current value(s) of the specified timer(s)
 * @brief	MUST do a SysTimerStop() before calling to freeze accurate value in array
 * @param	tMask 8bit bitmapped flag to select timer(s) to display
 * @return	none
 */
void	vSysTimerShow(uint32_t TimerMask) {
	uint32_t	Mask, TimNum ;
	systimer_t * pST ;
	bool	HdrDone ;
	for (TimNum = 0, pST = STdata, Mask = 0x00000001, HdrDone = 0; TimNum < systimerMAX_NUM; ++TimNum, ++pST) {
		if ((TimerMask & Mask) && pST->Count) {
			if (SYSTIMER_TYPE(TimNum)) {
				if (HdrDone == 0) { xprintf(systimerHDR_CLOCKS) ; HdrDone = 1 ; }
				xprintf("|%2d |%8s|%'#7u|%'#7u|%'#7u|",
					TimNum,
					pST->Tag,
					pST->Count,
					myCLOCKS_TO_US(pST->Last, uint32_t),
					myCLOCKS_TO_US(pST->Min, uint32_t)) ;
				xprintf("%'#7u|%'#7llu|%'#7llu|%'#7u|",
					myCLOCKS_TO_US(pST->Max, uint32_t),
					myCLOCKS_TO_US(pST->Sum, uint64_t) / (uint64_t) pST->Count,
					myCLOCKS_TO_US(pST->Sum, uint64_t),
					pST->Last) ;
				xprintf("%'#7u|%'#7u|%'#7llu|%'#7llu|",
					pST->Min,
					pST->Max,
					pST->Sum / (uint64_t) pST->Count,
					pST->Sum) ;
#if		(systimerSCATTER == 1)
				vSysTimerShowScatter(pST) ;
#endif
				xprintf("\n") ;
			}
		}
		Mask <<= 1 ;
	}

	for (TimNum = 0, pST = STdata, Mask = 0x00000001, HdrDone = 0; TimNum < systimerMAX_NUM; ++TimNum, ++pST) {
		if ((TimerMask & Mask) && pST->Count) {
			if (!SYSTIMER_TYPE(TimNum)) {
				if (HdrDone == 0) { xprintf(systimerHDR_TICKS) ; HdrDone = 1 ; }
				xprintf("|%2d |%8s|%'#7u|%'#7u|%'#7u|",
					TimNum,
					pST->Tag,
					pST->Count,
					myTICKS_TO_MS(pST->Last, uint32_t),
					myTICKS_TO_MS(pST->Min, uint32_t)) ;
				xprintf("%'#7u|%'#7llu|%'#7llu|",
					myTICKS_TO_MS(pST->Max, uint32_t),
					myTICKS_TO_MS(pST->Sum, uint64_t) / (uint64_t) pST->Count,
					myTICKS_TO_MS(pST->Sum, uint64_t)) ;
#if		(systimerSCATTER == 1)
				vSysTimerShowScatter(pST) ;
#endif
				xprintf("\n") ;
			}
		}
		Mask <<= 1 ;
	}
	xprintf("\n") ;
}

#endif


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

void	vSysTimingTestSet(uint32_t Type, const char * Tag, uint32_t Delay) {
	vSysTimerReset(0xFFFFFFFF, Type, Tag, myMS_TO_TICKS(Delay), myMS_TO_TICKS(Delay * systimerSCATTER_GROUPS)) ;
	for (uint32_t Steps = 0; Steps <= systimerSCATTER_GROUPS; ++Steps) {
		for (uint32_t Count = 0; Count < systimerMAX_NUM; xSysTimerStart(Count++)) ;
		vTaskDelay(pdMS_TO_TICKS((Delay * Steps) + 1)) ;
		for (uint32_t Count = 0; Count < systimerMAX_NUM; xSysTimerStop(Count++)) ;
	}
	vSysTimerShow(0xFFFF) ;
}

void	vSysTimingTest(void) {
#if 0
	uint32_t	uCount, uSecs ;
	// Test the uSec delays
	uCount	= GET_CLOCK_COUNTER() ;
	uSecs	= xClockDelayUsec(100) ;
	SL_DBG("Delay=%'u uS", (uSecs - uCount) / configCLOCKS_PER_USEC) ;

	uCount	= GET_CLOCK_COUNTER() ;
	uSecs	= xClockDelayUsec(1000) ;
	SL_DBG("Delay=%'u uS", (uSecs - uCount) / configCLOCKS_PER_USEC) ;

	uCount	= GET_CLOCK_COUNTER() ;
	uSecs	= xClockDelayUsec(10000) ;
	SL_DBG("Delay=%'u uS", (uSecs - uCount) / configCLOCKS_PER_USEC) ;
#endif
#if 1
	// Test TICK timers & Scatter groups
	vSysTimingTestSet(systimerTICKS, "TICKS", 1) ;
	vSysTimingTestSet(systimerTICKS, "TICKS", 10) ;
	vSysTimingTestSet(systimerTICKS, "TICKS", 100) ;
	vSysTimingTestSet(systimerTICKS, "TICKS", 1000) ;
#endif
#if 0
	// Test CLOCK timers & Scatter groups
	vSysTimingTestSet(systimerCLOCKS, "CLOCKS", 1) ;
	vSysTimingTestSet(systimerCLOCKS, "CLOCKS", 10) ;
	vSysTimingTestSet(systimerCLOCKS, "CLOCKS", 100) ;
	vSysTimingTestSet(systimerCLOCKS, "CLOCKS", 1000) ;
#endif
}
