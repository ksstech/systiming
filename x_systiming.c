/*
 * Copyright 2014-19 Andre M Maree / KSS Technologies (Pty) Ltd.
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
 */

/*
 * 	x_systiming.c
 */

#include	"FreeRTOS_Support.h"

#include	"x_syslog.h"
#include	"x_systiming.h"

#include	"hal_config.h"
#include	"hal_debug.h"
#include	"hal_timer.h"

#include	<string.h>

#define	debugFLAG					0x0000

#define	debugPARAM					(debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG & 0x8000)

// ################################# Code execution timer support ##################################

#define	SYSTIMER_TYPE(x)	(STtype & (1UL << x))

static systimer_t	STdata[systimerMAX_NUM] = { 0 } ;
static uint32_t		STstat = 0, STtype = 0 ;
#if		(ESP32_PLATFORM == 1) && !defined(CONFIG_FREERTOS_UNICORE)
	static uint32_t	STcore = 0 ;
	uint32_t		STskip = 0 ;
#endif

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
 * vSysTimerResetCounters() -Reset all the timer values for a single timer #
 * @brief 	This function does NOT reset SGmin & SGmax. To reset Min/Max use vSysTimerInit()
 * @brief	which allows for the type to be changed as well as specifying new Min/Max values.
 * @param 	TimNum
 */
void	vSysTimerResetCounters(uint8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < systimerMAX_NUM) ;
	systimer_t *pST	= &STdata[TimNum] ;
	STstat			&= ~(1UL << TimNum) ;					// clear active status ie STOP
	pST->Sum		= 0ULL ;
	pST->Min		= UINT32_MAX ;
	pST->Last		= pST->Count	= pST->Max	= 0 ;
#if		(systimerSCATTER == 1)
	memset(&pST->Group, 0, SIZEOF_MEMBER(systimer_t, Group)) ;
#endif
}

/**
 * vSysTimerResetCountersMask() - Reset all the timer values for 1 or more timers
 * @brief 	This function does NOT reset SGmin & SGmax. To reset Min/Max use vSysTimerInit()
 * @brief	which allows for the type to be changed as well as specifying new Min/Max values.
 * @param	TimerMask
 */
void	vSysTimerResetCountersMask(uint32_t TimerMask) {
	uint32_t mask 	= 0x00000001 ;
	systimer_t *pST	= STdata ;
	for (uint8_t TimNum = 0; TimNum < systimerMAX_NUM; ++TimNum, ++pST) {
		if (TimerMask & mask) {
			vSysTimerResetCounters(TimNum) ;
		}
		mask <<= 1 ;
	}
}

void	vSysTimerInit(uint8_t TimNum, bool Type, const char * Tag, ...) {
	IF_myASSERT(debugPARAM, TimNum < systimerMAX_NUM) ;
	systimer_t *pST	= &STdata[TimNum] ;
	pST->Tag	= Tag ;
	if (Type) {
		STtype |= (1UL << TimNum) ;						// mark as CLOCK type
	} else {
		STtype &= ~(1UL << TimNum) ;					// mark as TICK type
	}
	vSysTimerResetCounters(TimNum) ;
#if		(systimerSCATTER == 1)
    va_list vaList ;
    va_start(vaList, Tag) ;
	pST->SGmin	= va_arg(vaList, uint32_t) ;			// Min
	pST->SGmax	= va_arg(vaList, uint32_t) ;			// Max ;
	IF_myASSERT(debugPARAM, pST->SGmin < pST->SGmax) ;
	va_end(vaList) ;
#endif
}

/**
 * xSysTimerStart() - start the specified timer
 * @param 		TimNum
 * @return		current timer value based on type (CLOCKs or TICKs)
 */
uint32_t xSysTimerStart(uint8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < systimerMAX_NUM) ;
#if		(ESP32_PLATFORM == 1) && !defined(CONFIG_FREERTOS_UNICORE)
	if (xPortGetCoreID()) {
		STcore |= (1UL << TimNum) ;						// Running on Core 1
	} else {
		STcore &= ~(1UL << TimNum) ;					// Running on Core 0
	}
#endif
	STstat	|= (1UL << TimNum) ;						// Mark as started & running
	return (STdata[TimNum].Last = SYSTIMER_TYPE(TimNum) ? GET_CLOCK_COUNTER() : xTaskGetTickCount()) ;
}

/**
 * xSysTimerStop() stop the specified timer and update the statistics
 * @param	TimNum
 * @return	Last measured interval based on type (CLOCKs or TICKs)
 */
uint32_t xSysTimerStop(uint8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < systimerMAX_NUM) ;
	uint32_t tNow		= SYSTIMER_TYPE(TimNum) ? GET_CLOCK_COUNTER() : xTaskGetTickCount() ;
	STstat				&= ~(1UL << TimNum) ;				// mark as stopped

	#if	(ESP32_PLATFORM == 1) && !defined(CONFIG_FREERTOS_UNICORE)
	/* Adjustments made to CCOUNT cause discrepancies between readings from different cores.
	 * In order to filter out invalid/OOR values we verify whether the timer is being stopped
	 * on the same MCU as it was started. If not, we ignore the timing values
	 */
	uint8_t	xCoreID		= (STcore & (1UL << TimNum)) ? 1 : 0 ;
	if (SYSTIMER_TYPE(TimNum) && xCoreID != xPortGetCoreID()) {
		++STskip ;
		return 0 ;
	}
	#endif
	systimer_t *pST	= &STdata[TimNum] ;
	uint32_t tElap	= tNow - pST->Last ;
	pST->Last		= tElap ;
	pST->Sum		+= tElap ;
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
	IF_PRINT(debugRESULT && OUTSIDE(0, Idx, systimerSCATTER_GROUPS-1, int32_t), "l=%u h=%u n=%u i=%d\n", pST->SGmin, pST->SGmax, pST->Last, Idx) ;
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
	printfx(Header) ;
	#if		(systimerSCATTER == 1)
	for (int32_t Idx = 0; Idx < systimerSCATTER_GROUPS; ++Idx) {
		printfx("%-6u+|", Idx == 0 ? 0 :
									Idx == 1 ? pST->SGmin + 1 :
									Idx == systimerSCATTER_GROUPS-1 ? pST->SGmax :
									pST->SGmin + (pST->SGfact * (Idx-1)) + 1) ;
	}
	#endif
	printfx("\n") ;
}

void	vSysTimerShowScatter(systimer_t * pST) { for (int32_t Idx = 0; Idx < systimerSCATTER_GROUPS; printfx("%7u|", pST->Group[Idx++])) ; }

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
				printfx("|%2d |%8s|%'9u|%'12llu|%'14llu|",
					TimNum,
					pST->Tag,
					pST->Count,
					myCLOCKS_TO_US(pST->Sum, uint64_t) / (uint64_t) pST->Count,
					myCLOCKS_TO_US(pST->Sum, uint64_t)) ;
				printfx("%'12u|%'12u|%'12u|%'14u|",
					myCLOCKS_TO_US(pST->Last, uint32_t),
					myCLOCKS_TO_US(pST->Min, uint32_t),
					myCLOCKS_TO_US(pST->Max, uint32_t),
					pST->Sum / (uint64_t) pST->Count) ;
				printfx("%'16u|%'14u|%'14u|%'14u|",
					pST->Sum, pST->Last, pST->Min, pST->Max) ;
			} else {
				printfx("|%2d |%8s|%'9u|%'10llu|%'12llu|",
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
			IF_EXEC_1(systimerSCATTER == 1, vSysTimerShowScatter, pST) ;
			xdprintf(Handle, "\n\n") ;
		}
		Mask <<= 1 ;
	}
}
#else

#define	systimerHDR_TICKS	"| # |TickTMR | Count |Last mS|Min mS |Max mS |Avg mS |Sum mS |"
#define	systimerHDR_CLOCKS	"| # |ClockTMR| Count |Last uS|Min uS |Max uS |Avg uS |Sum uS |LastClk|Min Clk|Max Clk|Avg Clk|Sum Clk|"

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
			printfx("  #%d:%'#u->%'#u=%'#u", Idx, Rlo, Rhi, pST->Group[Idx]) ;
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
	// first handle the simpler tick timers
	for (TimNum = 0, pST = STdata, Mask = 0x00000001, HdrDone = 0; TimNum < systimerMAX_NUM; ++TimNum, ++pST) {
		if ((TimerMask & Mask) && pST->Count) {
			if (!SYSTIMER_TYPE(TimNum)) {
				if (HdrDone == 0) {
					printfx("%C%s%C\n", xpfSGR(attrRESET, colourFG_CYAN, 0, 0), systimerHDR_TICKS, attrRESET) ;
					HdrDone = 1 ;
				}
				printfx("|%2d%c|%8s|%'#7u|%'#7u|%'#7u|",
					TimNum,
					STstat & (1UL << TimNum) ? 'R' : ' ',
					pST->Tag,
					pST->Count,
					myTICKS_TO_MS(pST->Last, uint32_t),
					myTICKS_TO_MS(pST->Min, uint32_t)) ;
				printfx("%'#7u|%'#7llu|%'#7llu|",
					myTICKS_TO_MS(pST->Max, uint32_t),
					myTICKS_TO_MS(pST->Sum, uint64_t) / (uint64_t) pST->Count,
					myTICKS_TO_MS(pST->Sum, uint64_t)) ;
				IF_EXEC_1(systimerSCATTER == 1, vSysTimerShowScatter, pST) ;
				printfx("\n") ;
			}
		}
		Mask <<= 1 ;
	}
	// Now handle the clock timers
	for (TimNum = 0, pST = STdata, Mask = 0x00000001, HdrDone = 0; TimNum < systimerMAX_NUM; ++TimNum, ++pST) {
		if ((TimerMask & Mask) && pST->Count) {
			if (SYSTIMER_TYPE(TimNum)) {
				if (HdrDone == 0) {
#if		(ESP32_PLATFORM == 1) && !defined(CONFIG_FREERTOS_UNICORE)
					printfx("%C%s%C OOR Skipped %u\n", xpfSGR(attrRESET, colourFG_CYAN, 0, 0), systimerHDR_CLOCKS, attrRESET, STskip) ;
#else
					printfx("%C%s%C\n", xpfSGR(attrRESET, colourFG_CYAN, 0, 0), systimerHDR_CLOCKS, attrRESET) ;
#endif
					HdrDone = 1 ;
				}
				printfx("|%2d%c|%8s|%'#7u|%'#7u|%'#7u|",
					TimNum,
					STstat & (1UL << TimNum) ? 'R' : ' ',
					pST->Tag,
					pST->Count,
					myCLOCKS_TO_US(pST->Last, uint32_t),
					myCLOCKS_TO_US(pST->Min, uint32_t)) ;
				printfx("%'#7u|%'#7llu|%'#7llu|%'#7u|",
					myCLOCKS_TO_US(pST->Max, uint32_t),
					myCLOCKS_TO_US(pST->Sum, uint64_t) / (uint64_t) pST->Count,
					myCLOCKS_TO_US(pST->Sum, uint64_t),
					pST->Last) ;
				printfx("%'#7u|%'#7u|%'#7llu|%'#7llu|",
					pST->Min,
					pST->Max,
					pST->Sum / (uint64_t) pST->Count,
					pST->Sum) ;
				IF_EXEC_1(systimerSCATTER == 1, vSysTimerShowScatter, pST) ;
				printfx("\n") ;
			}
		}
		Mask <<= 1 ;
	}
	printfx("\n") ;
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

#define	systimerTEST_DELAY			0
#define	systimerTEST_TICKS			0
#define	systimerTEST_CLOCKS			0

void	vSysTimingTestSet(uint32_t Type, const char * Tag, uint32_t Delay) {
	for (uint8_t Idx = 0; Idx < systimerMAX_NUM; ++Idx) {
		vSysTimerInit(Idx, Type, Tag, myMS_TO_TICKS(Delay), myMS_TO_TICKS(Delay * systimerSCATTER_GROUPS)) ;
	}
	for (uint32_t Steps = 0; Steps <= systimerSCATTER_GROUPS; ++Steps) {
		for (uint32_t Count = 0; Count < systimerMAX_NUM; xSysTimerStart(Count++)) ;
		vTaskDelay(pdMS_TO_TICKS((Delay * Steps) + 1)) ;
		for (uint32_t Count = 0; Count < systimerMAX_NUM; xSysTimerStop(Count++)) ;
	}
	vSysTimerShow(0xFFFFFFFF) ;
}

void	vSysTimingTest(void) {
#if		(systimerTEST_DELAY == 1)						// Test the uSec delays
	uint32_t	uCount, uSecs ;
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
#if 	(systimerTEST_TICKS == 1)						// Test TICK timers & Scatter groups
	vSysTimingTestSet(systimerTICKS, "TICKS", 1) ;
	vSysTimingTestSet(systimerTICKS, "TICKS", 10) ;
	vSysTimingTestSet(systimerTICKS, "TICKS", 100) ;
	vSysTimingTestSet(systimerTICKS, "TICKS", 1000) ;
#endif
#if 	(systimerTEST_CLOCKS == 1)						// Test CLOCK timers & Scatter groups
	vSysTimingTestSet(systimerCLOCKS, "CLOCKS", 1) ;
	vSysTimingTestSet(systimerCLOCKS, "CLOCKS", 10) ;
	vSysTimingTestSet(systimerCLOCKS, "CLOCKS", 100) ;
	vSysTimingTestSet(systimerCLOCKS, "CLOCKS", 1000) ;
#endif
}
