/*
 * systiming.c
 * Copyright 2014-20 Andre M Maree / KSS Technologies (Pty) Ltd.
 */

#include	"systiming.h"
#include	"printfx.h"									// +x_definitions +stdarg +stdint +stdio
#include	"hal_config.h"

#if		defined(ESP_PLATFORM)
	#include	"FreeRTOS_Support.h"
#endif

#include	<string.h>

#define	debugFLAG					0xC000

#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// ################################# Code execution timer support ##################################

#define	SYSTIMER_TYPE(x)	(STtype & (1UL << x))

static systimer_t	STdata[systimerMAX_NUM] = { 0 } ;
static uint32_t		STstat = 0, STtype = 0 ;
#if		defined(ESP_PLATFORM) && !defined(CONFIG_FREERTOS_UNICORE)
	static uint32_t	STcore = 0 ;
	uint32_t		STskip = 0 ;
#endif

/**
 * vSysTimerResetCounters() -Reset all the timer values for a single timer #
 * @brief 	This function does NOT reset SGmin & SGmax. To reset Min/Max use vSysTimerInit()
 * @brief	which allows for the type to be changed as well as specifying new Min/Max values.
 * @param 	TimNum
 */
void	vSysTimerResetCounters(uint8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < systimerMAX_NUM) ;
	IF_TRACK(debugTRACK, "#=%d", TimNum) ;
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
		if (TimerMask & mask)	vSysTimerResetCounters(TimNum) ;
		mask <<= 1 ;
	}
}

void	vSysTimerInit(uint8_t TimNum, bool Type, const char * Tag, ...) {
	IF_myASSERT(debugPARAM, TimNum < systimerMAX_NUM) ;
	IF_TRACK(debugTRACK, "#=%d  T=%d '%s'", TimNum, Type, Tag) ;
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
#if		defined(ESP_PLATFORM) && !defined(CONFIG_FREERTOS_UNICORE)
	if (xPortGetCoreID()) {
		STcore |= (1UL << TimNum) ;						// Running on Core 1
	} else {
		STcore &= ~(1UL << TimNum) ;					// Running on Core 0
	}
#endif
	STstat	|= (1UL << TimNum) ;						// Mark as started & running
	STdata[TimNum].Count++ ;
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

	#if	defined(ESP_PLATFORM) && !defined(CONFIG_FREERTOS_UNICORE)
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
	// update Min & Max if required
	if (tElap < pST->Min) {
		pST->Min	=  tElap ;
	}
	if (tElap > pST->Max) {
		pST->Max	=  tElap ;
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
	uint32_t	tNow = 0 ;
	if (STstat & (1 << TimNum)) {
		tNow = SYSTIMER_TYPE(TimNum) ? GET_CLOCK_COUNTER() : xTaskGetTickCount() ;
		systimer_t * pST = &STdata[TimNum] ;
		if (SYSTIMER_TYPE(TimNum)) {
			if (tNow > pST->Last) {							// from here outside of running timer context
				tNow -= pST->Last ;							// most likely NO wrap
			} else {
				tNow += (0xFFFFFFFF - pST->Last) ;			// definitely wrapped
			}
		} else {
			tNow -= pST->Last ;
		}
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
	IF_myASSERT(debugPARAM, TimNum < systimerMAX_NUM && halCONFIG_inSRAM(pST)) ;
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

void	vSysTimerShowScatter(systimer_t * pST) {
	for (int32_t Idx = 0; Idx < systimerSCATTER_GROUPS; printfx("%7u|", pST->Group[Idx++])) ;
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
			vSysTimerShowHeading(SYSTIMER_TYPE(TimNum) ? systimerHDR_CLOCKS : systimerHDR_TICKS, pST) ;
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
				printfx("%'10u|%'10u|%'10u|",
					myTICKS_TO_MS(pST->Last, uint32_t),
					pST->Min,
					pST->Max) ;
			}
			IF_EXEC_1(systimerSCATTER == 1, vSysTimerShowScatter, pST) ;
			printfx("\n\n") ;
		}
		Mask <<= 1 ;
	}
}
#else

#define	systimerHDR_TICKS	"| # |TickTMR | Count |Last mS|Min mS |Max mS |Avg mS |Sum mS |"
#define	systimerHDR_CLOCKS	"| # |ClockTMR| Count |Last uS|Min uS |Max uS |Avg uS |Sum uS |LastClk|Min Clk|Max Clk|Avg Clk|Sum Clk|"

#if		(systimerSCATTER == 1)
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
#endif

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
					printfx("\n%C%s%C\n", xpfSGR(colourFG_CYAN, 0, 0, 0), systimerHDR_TICKS, xpfSGR(attrRESET, 0, 0, 0)) ;
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
#if		(systimerSCATTER == 1)
				vSysTimerShowScatter(pST) ;
#endif
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
#if		defined(ESP_PLATFORM) && defined(CONFIG_FREERTOS_UNICORE)
					printfx("\n%C%s%C\n", xpfSGR(colourFG_CYAN, 0, 0, 0), systimerHDR_CLOCKS, xpfSGR(attrRESET, 0, 0, 0)) ;
#else
					printfx("\n%C%s%C OOR Skipped %u\n", xpfSGR(colourFG_CYAN, 0, 0, 0), systimerHDR_CLOCKS, xpfSGR(attrRESET, 0, 0, 0), STskip) ;
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
#if		(systimerSCATTER == 1)
				vSysTimerShowScatter(pST) ;
#endif
				printfx("\n") ;
			}
		}
		Mask <<= 1 ;
	}
}
#endif

int64_t	i64TaskDelayUsec(uint32_t u32Period) {
	int64_t	i64Start = esp_timer_get_time() ;
	int64_t	i64Period = u32Period ;
	int64_t i64Now ;
	UBaseType_t CurPri = uxTaskPriorityGet(NULL) ;
	vTaskPrioritySet(NULL, 0) ;
	while ((i64Now = esp_timer_get_time() - i64Start) < i64Period)	taskYIELD() ;
	vTaskPrioritySet(NULL, CurPri) ;
	IF_PRINT(debugTIMING, "D=%lli   ", i64Now - i64Period) ;
	return i64Now ;
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

#define	systimerTEST_DELAY			1
#define	systimerTEST_TICKS			1
#define	systimerTEST_CLOCKS			1

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
	uint32_t	uClock, uSecs ;
	uClock	= GET_CLOCK_COUNTER() ;
	uSecs	= xClockDelayUsec(100) ;
	printfx("Delay=%'u uS\n", (uSecs - uClock) / configCLOCKS_PER_USEC) ;

	uClock	= GET_CLOCK_COUNTER() ;
	uSecs	= xClockDelayUsec(1000) ;
	printfx("Delay=%'u uS\n", (uSecs - uClock) / configCLOCKS_PER_USEC) ;

	uClock	= GET_CLOCK_COUNTER() ;
	uSecs	= xClockDelayUsec(10000) ;
	printfx("Delay=%'u uS\n", (uSecs - uClock) / configCLOCKS_PER_USEC) ;
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
