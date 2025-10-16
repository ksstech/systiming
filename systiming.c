// systiming.c - Copyright (c) 2014-25 Andre M. Maree / KSS Technologies (Pty) Ltd.

#include "hal_platform.h"
#include "systiming.h"
#include "hal_memory.h"
#include "struct_union.h"
#include "syslog.h"
#include "FreeRTOS_Support.h"

#include "xtensa/hal.h"
#include "esp_timer.h"
#ifdef ESP_PLATFORM
	#include <rom/ets_sys.h>
#endif

#include <string.h>

#define	debugFLAG					0xF000
#define	debugINIT					(debugFLAG & 0x0001)

#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// ########################################## Macros ###############################################

#define	vSysTimerSetType(i,x)		maskSET2B(STtype,i,x,u64_t)
#define	xSysTimerGetType(i)			maskGET2B(STtype,i,u64_t)
#define xSysTimerGetTime(t)	(t==stCLOCKS ? xthal_get_ccount() : t==stMICROS ? esp_timer_get_time() : xTaskGetTickCount())

// #################################### Local static variables #####################################

static systimer_t STdata[stMAX_NUM] = { 0 };
static u64_t STtype = 0;								// type 0=stUNDEF
static u32_t STstat = 0;								// status 1=Running

#ifndef CONFIG_FREERTOS_UNICORE
	static u32_t STcore = 0;							// Core# 0/1
#endif

// ###################################### Private APIs #############################################

/**
 * @brief	Reset all the timer values for a single timer #
 * @brief 	This function does NOT reset SGmin & SGmax. To reset Min/Max use vSysTimerInit()
 * @brief	which allows for the type to be changed as well as specifying new Min/Max values.
 * @param 	TimNum
 */
static void vSysTimerResetCounter(u8_t TimNum) {
	systimer_t *pST	= &STdata[TimNum];
	STstat		&= ~(1UL << TimNum);					// clear active status ie STOP
	pST->Sum	= 0ULL;
	pST->Last	= 0;
	pST->Count	= 0;
	pST->Max	= 0;
	pST->Min	= 0xFFFFFFFF;
	#if	(systimerSCATTER > 2)
		memset(&pST->Group, 0, SO_MEM(systimer_t, Group));
	#endif
}

// ################################### Public Control APIs #########################################

void vSysTimerInit(u8_t TimNum, int Type, const char * Tag, ...) {
	IF_myASSERT(debugPARAM, TimNum < stMAX_NUM && INRANGE(stTICKS, Type, stCLOCKS));
	systimer_t *pST	= &STdata[TimNum];
	pST->Tag = Tag;
	vSysTimerSetType(TimNum, Type);
	vSysTimerResetCounter(TimNum);
	#if	(systimerSCATTER > 2)
    	va_list vaList;
    	va_start(vaList, Tag);
    	// Assume default type is stMICROS so values in uSec
		pST->SGmin	= va_arg(vaList, u32_t);
		pST->SGmax	= va_arg(vaList, u32_t);
		va_end(vaList);
		IF_myASSERT(debugPARAM, pST->SGmin < pST->SGmax);
	#endif
}

void vSysTimerDeInit(u8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < stMAX_NUM);
	systimer_t *pST	= &STdata[TimNum];
	pST->Tag = NULL;
	vSysTimerSetType(TimNum, stUNDEF);
	vSysTimerResetCounter(TimNum);
}

u32_t xSysTimerStart(u8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < stMAX_NUM);
	int Type = xSysTimerGetType(TimNum);
	#ifndef CONFIG_FREERTOS_UNICORE
	if (Type == stCLOCKS) {
		if (xPortGetCoreID()) {
			STcore |= (1UL<<TimNum);					// Running on Core 1
		} else {
			STcore &= ~(1UL << TimNum);					// Running on Core 0
		}
	}
	#endif
	STstat |= (1UL << TimNum);							// Mark as started & running
	++STdata[TimNum].Count;
	return STdata[TimNum].Last = xSysTimerGetTime(Type);
}

u32_t xSysTimerStop(u8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < stMAX_NUM);
	int Type = xSysTimerGetType(TimNum);
	u32_t tNow = xSysTimerGetTime(Type);				// capture stop time as early as possible
	STstat &= ~(1UL << TimNum);							//  mark timer as stopped
	systimer_t *pST	= &STdata[TimNum];
	/* Adjustments made to CCOUNT cause discrepancies between readings from different cores.
	 * In order to filter out invalid/OOR values we verify whether the timer is being stopped
	 * on the same MCU as it was started. If not, we ignore the timing values */
	#ifndef CONFIG_FREERTOS_UNICORE
	if (Type == stCLOCKS) {
		int xCoreID = (STcore & (1UL << TimNum)) ? 1 : 0;
		if (xCoreID != xPortGetCoreID()) {
			++pST->Skip;
			return 0;
		}
	}
	#endif
	u32_t tElap = tNow - pST->Last;						// cal culate elapsed time
	pST->Sum += tElap;									// update sum of all times
	pST->Last = tElap;									// and save as previous/last time
	// update Min & Max if required
	if (pST->Min > tElap)								// if required
		pST->Min = tElap;								// update new minimum
	if (pST->Max < tElap)
		pST->Max = tElap;								// and/or new maximum
	#if	(systimerSCATTER > 2)
		int Idx;
		if (tElap <= pST->SGmin) {						// LE minimum ?
			Idx = 0;									// first bucket
		} else if (tElap >= pST->SGmax) {				// GE maximum ?	
			Idx = systimerSCATTER-1;					// last bucket
		} else {										// anything inbetween
			u32_t tBlock = (pST->SGmax - pST->SGmin) / (systimerSCATTER - 2);
			u32_t tDiff = tElap - pST->SGmin;
			Idx = 1 + (tDiff/tBlock);					// calculate bucket number/index
		}
		if (INRANGE(0, Idx, systimerSCATTER-1))	{
			++pST->Group[Idx];							// update bucket count
		} else {
			SL_CRIT("l=%lu h=%lu n=%lu i=%d", pST->SGmin, pST->SGmax, tElap, Idx);
		}
		IF_myASSERT(debugRESULT, INRANGE(0, Idx, systimerSCATTER-1));
	#endif
	return tElap;
}

u32_t xSysTimerToggle(u8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < stMAX_NUM);
	return (STstat & (1 << TimNum)) ? xSysTimerStop(TimNum) : xSysTimerStart(TimNum);
}

void vSysTimerResetCountersMask(u32_t TimerMask) {
	u32_t mask = 0x00000001;
	systimer_t *pST	= STdata;
	for (u8_t TimNum = 0; TimNum < stMAX_NUM; ++TimNum, ++pST) {
		if (TimerMask & mask)
			vSysTimerResetCounter(TimNum);
		mask <<= 1;
	}
}

// ################################### Public Status APIs ##########################################

u32_t xSysTimerIsRunning(u8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < stMAX_NUM);
	u32_t tNow = 0;
	if (STstat & (1 << TimNum)) {
		int Type = xSysTimerGetType(TimNum);
		IF_myASSERT(debugPARAM, Type < stMAX_TYPE);
		tNow = xSysTimerGetTime(Type);
		systimer_t * pST = &STdata[TimNum];
		if (Type == stCLOCKS) {
			if (tNow > pST->Last) {
				tNow -= pST->Last;						// Unlikely wrapped
			} else { 
				tNow += (0xFFFFFFFF - pST->Last);		// definitely wrapped
			}
		} else {
			tNow -= pST->Last;
		}
	}
	return tNow;
}

int	xSysTimerGetStatus(u8_t TimNum, systimer_t * pST) {
	IF_myASSERT(debugPARAM, TimNum < stMAX_NUM && halMemorySRAM((void*) pST));
	memcpy(pST, &STdata[TimNum], sizeof(systimer_t));
	return xSysTimerGetType(TimNum);
}

// #################################### Elapsed time APIs ##########################################

u64_t xSysTimerGetElapsedClocks(u8_t TimNum) {
	IF_myASSERT(debugPARAM, (TimNum < stMAX_NUM) && xSysTimerGetType(TimNum) > stMICROS);
	return STdata[TimNum].Sum;
}

u64_t xSysTimerGetElapsedMicros(u8_t TimNum) {
	IF_myASSERT(debugPARAM, (TimNum < stMAX_NUM) && xSysTimerGetType(TimNum) > stTICKS);
	u64_t tElap = STdata[TimNum].Sum;
	tElap = xSysTimerGetType(TimNum) == stMICROS ? tElap : u64ClocksToUSec(tElap);
	return tElap;
}

u64_t xSysTimerGetElapsedMillis(u8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < stMAX_NUM);
	int Type = xSysTimerGetType(TimNum);
	IF_myASSERT(debugPARAM, Type < stMAX_TYPE);
	u64_t tElap = STdata[TimNum].Sum;
	tElap = Type==stTICKS ? u64TicksToMSec(tElap) : Type==stMICROS ? u64USecToMSec(tElap): u64ClocksToMSec(tElap);
	return tElap;
}

u64_t xSysTimerGetElapsedSecs(u8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < stMAX_NUM);
	int Type = xSysTimerGetType(TimNum);
	IF_myASSERT(debugPARAM, Type < stMAX_TYPE);
	u64_t tElap = STdata[TimNum].Sum;
	tElap =  Type==stTICKS ? u64TicksToSec(tElap) : Type==stMICROS ? u64USecToMSec(tElap) : u64ClocksToSec(tElap);
	return tElap;
}

// ################################## Timer status reporting #######################################

#if (CONFIG_FREERTOS_HZ == MILLIS_IN_SECOND)
#define	stHDR_MILLIS	" mS |Min mS |Max mS |Avg mS |Sum mS |"
#else
#define	stHDR_MILLIS	"Tick|MinTick|MaxTick|AvgTick|SumTick|"
#endif
#define	stHDR_MICROS	" uS |Min uS |Max uS |Avg uS |Sum uS |"
#define	stHDR_CLOCKS	"Clk |Min Clk|Max Clk|Avg Clk|Sum Clk|"
#define stHDR_FMT1		"%C| # |  Name  | Count |Prv%s%C"
#define stHDR_FMT2		"X-MCU-Y|"
#define stDTL_FMT1		"|%2d%c|%8s|%#'7lu|"
#define stDTL_FMT2		"%#'7lu|%#'7lu|%#'7lu|%#'7lu|%#'7llu|"

void vSysTimerShow(report_t * psR, u32_t TimerMask) {
	const char * pcTag;
	char caTmp[12];
	for (int Type = stTICKS; Type < stMAX_TYPE; ++Type) {			// order tabled output by type
		u32_t Mask = 0x00000001;									// start with lowest timer number
		int HdrDone = 0;											// Ensure header output per type
		for (int Num = 0; Num < stMAX_NUM; Mask <<= 1, ++Num) {		// loop through all timers
			systimer_t * pST = &STdata[Num];
			if ((TimerMask & Mask) && (Type == xSysTimerGetType(Num)) && pST->Count) {	// check timer, type & count
				if (HdrDone == 0) {										// if header not done for this type
					xReport(psR, stHDR_FMT1, xpfCOL(colourFG_CYAN,0),	// report type specific header
						(Type == stTICKS) ? stHDR_MILLIS :
						(Type == stMICROS) ? stHDR_MICROS : stHDR_CLOCKS,
						xpfCOL(attrRESET,0));
					#ifndef CONFIG_FREERTOS_UNICORE
						if (Type == stCLOCKS)				// add CLOCK specific header info
							xReport(psR, stHDR_FMT2);
					#endif
					xReport(psR, strNL);				// termimate header
					HdrDone = 1;						// mark as being done
				}
				if (halMemoryANY((void *)pST->Tag)) {	// if tag provided
					pcTag = pST->Tag;					// use it
				} else {								// else fabricate a tab
					snprintfx(caTmp, sizeof(caTmp), "T#%d+%d", pST->Tag, Num - (int)pST->Tag);
					pcTag = caTmp;						// use fabricated tab
				}
				xReport(psR, stDTL_FMT1, Num, (STstat & (1UL << Num)) ? 'R' : ' ', pcTag, pST->Count);
				xReport(psR, stDTL_FMT2, pST->Last, pST->Min, pST->Max,(u32_t) (pST->Count ? (pST->Sum / pST->Count) : pST->Sum), pST->Sum);
				#ifndef CONFIG_FREERTOS_UNICORE
					if (Type == stCLOCKS)				// add CLOCK specific details
						xReport(psR, "%#'7lu|", pST->Skip);
				#endif
				#if	(systimerSCATTER > 2)	// add scatter info
					u32_t Rlo, Rhi;
					for (int Idx = 0; Idx < systimerSCATTER; ++Idx) {
						if (pST->Group[Idx]) {
							if (Idx == 0) {
								Rlo = 0;
								Rhi = pST->SGmin;
							} else if (Idx == (systimerSCATTER-1)) {
								Rlo = pST->SGmax;
								Rhi = 0xFFFFFFFF;
							} else {
								u32_t Rtmp = (pST->SGmax - pST->SGmin) / (systimerSCATTER-2);
								Rlo	= ((Idx - 1) * Rtmp) + pST->SGmin;
								Rhi = Rlo + Rtmp;
							}
							xReport(psR, "  %d:%#'lu~%#'lu=%#'lu", Idx, Rlo, Rhi, pST->Group[Idx]);
						}
					}
				#endif
				xReport(psR, strNL);		// end of scatter groups for specific timer
			}
		}
	}
	xReport(psR, strNL);
}

// ################################### RTOS + HW delay support #####################################

i64_t i64TaskDelayUsec(u32_t u32Period) {
	i64_t i64Start = esp_timer_get_time();
	if (u32Period < 2)
		return esp_timer_get_time() - i64Start;
	i64_t i64Period = u32Period;
	i64_t i64Now;
	UBaseType_t CurPri = uxTaskPriorityGet(NULL);
	vTaskPrioritySet(NULL, 0);
	while ((i64Now = esp_timer_get_time() - i64Start) < i64Period) 
		taskYIELD();
	vTaskPrioritySet(NULL, CurPri);
	return i64Now;
}

// ################################## MCU Clock cycle delay support ################################

void vClockDelayUsec(u32_t uSec) {
#ifdef ESP_PLATFORM
	ets_delay_us(uSec);
#else
	IF_myASSERT(debugPARAM, uSec < (UINT32_MAX / configCLOCKS_PER_USEC));
	u32_t ClockEnd	= xthal_get_ccount() + (uSec * (u32_t)configCLOCKS_PER_USEC);
	while ((ClockEnd - xthal_get_ccount()) > configCLOCKS_PER_USEC );
#endif
}

void vClockDelayMsec(u32_t mSec) {
	IF_myASSERT(debugPARAM, mSec < (UINT32_MAX / configCLOCKS_PER_MSEC));
	vClockDelayUsec(mSec * MICROS_IN_MILLISEC);
}

// ##################################### functional tests ##########################################

#define	systimerTEST_DELAY			(systimerTESTFLAG & 0x0001)
#define	systimerTEST_MILLIS			(systimerTESTFLAG & 0x0002)
#define	systimerTEST_MICROS			(systimerTESTFLAG & 0x0004)
#define	systimerTEST_CLOCKS			(systimerTESTFLAG & 0x0008)
#define	systimerTEST_MACROS			(systimerTESTFLAG & 0x0010)
#define systimerTESTFLAG			0x0010
#define	systimerINTERVAL			1000

#if	(systimerTESTFLAG & 0x000E)
static void vSysTimingTestSet(u32_t Type, char * Tag, u32_t Delay) {
	// Adjust Delay for different units
	xReport(NULL, "Delay %lu", Delay);
	Delay = (Type == stTICKS) ? (Delay * 1000) : (Type == stMICROS) ? Delay : (Delay / configCLOCKS_PER_USEC);
	xReport(NULL, "-> %lu" strNL, Delay);
	vSysTimerInit(0, Type, Tag, Delay, Delay * systimerSCATTER);
	for (int SI = 0; SI < systimerSCATTER; ++SI) {
		xSysTimerStart(0);
		vClockDelayUsec(Delay * (SI + 1));
		xSysTimerStop(0);
	}
	vSysTimerShow(NULL, 1);
	vSysTimerDeInit(0);
	vTaskDelay(pdMS_TO_TICKS(100));
}
#endif

void vSysTimingTest(void) {
#if	(systimerTEST_DELAY)								/* Test the uSec delays */
	u32_t uClock, uSecs;
	uClock = xthal_get_ccount();
	vClockDelayUsec(100);
	uSecs = xthal_get_ccount();
	xReport(NULL, "Delay=%'lu uS" strNL, (uSecs - uClock) / configCLOCKS_PER_USEC);

	vTaskDelay(systimerINTERVAL);
	uClock = xthal_get_ccount();
	vClockDelayUsec(1000);
	uSecs = xthal_get_ccount();
	xReport(NULL, "Delay=%'lu uS" strNL, (uSecs - uClock) / configCLOCKS_PER_USEC);

	vTaskDelay(systimerINTERVAL);
	uClock = xthal_get_ccount();
	vClockDelayUsec(10000);
	uSecs = xthal_get_ccount();
	xReport(NULL, "Delay=%'lu uS" strNL, (uSecs - uClock) / configCLOCKS_PER_USEC);
#endif

#if (systimerTEST_MILLIS)								/* Test MILLIS timers & Scatter groups */
	vSysTimingTestSet(stTICKS, "MILLIS", 5);
	vSysTimingTestSet(stTICKS, "MILLIS", 25);
	vSysTimingTestSet(stTICKS, "MILLIS", 125);
#endif

#if (systimerTEST_MICROS)								/* Test MICROS timers & Scatter groups */
	vSysTimingTestSet(stMICROS, "MICROS", 100);
	vSysTimingTestSet(stMICROS, "MICROS", 1000);
	vSysTimingTestSet(stMICROS, "MICROS", 10000);
	vSysTimingTestSet(stMICROS, "MICROS", 100000);
#endif

#if (systimerTEST_CLOCKS)								/* Test CLOCKS timers & Scatter groups */
	vSysTimingTestSet(stCLOCKS, "CLOCKS", configCLOCKS_PER_USEC * 100);
	vSysTimingTestSet(stCLOCKS, "CLOCKS", configCLOCKS_PER_USEC * 1000);
	vSysTimingTestSet(stCLOCKS, "CLOCKS", configCLOCKS_PER_USEC * 10000);
	vSysTimingTestSet(stCLOCKS, "CLOCKS", configCLOCKS_PER_USEC * 100000);
#endif

#if (systimerTEST_MACROS)
	PX("Clocks -> Ticks:  Hz=%lu" strNL, CONFIG_FREERTOS_HZ);
	PX("\tIn=%lu  Out=%lu" strNL, 1600000, u32ClocksToTicks(1600000));
	PX("\tIn=%lu  Out=%lu" strNL, 160000000, u32ClocksToTicks(160000000));
	PX("\tIn=%lu  Out=%lu" strNL, 4200000000UL, u32ClocksToTicks(4200000000));
	// works up to 4200000000UL, UL MUST be specified
	PX("uSec -> Ticks:  Hz=%lu" strNL, CONFIG_FREERTOS_HZ);
	PX("\tIn=%lu  Out=%lu" strNL, 10000, u32USecToTicks(10000));
	PX("\tIn=%lu  Out=%lu" strNL, 1000000, u32USecToTicks(1000000));
	PX("\tIn=%lu  Out=%lu" strNL, 420000000, u32USecToTicks(420000000));
	// works up to 420000000 @ 100Hz
	PX("mSec -> Ticks:  Hz=%lu" strNL, CONFIG_FREERTOS_HZ);
	PX("\tIn=%lu  Out=%lu" strNL, 1000, u32MSecToTicks(1000));
	PX("\tIn=%lu  Out=%lu" strNL, 100000, u32MSecToTicks(100000));
	PX("\tIn=%lu  Out=%lu" strNL, 42000000, u32MSecToTicks(42000000));
	// works up to 42000000 @ 100Hz, 
	PX("Sec -> Ticks:  Hz=%lu" strNL, CONFIG_FREERTOS_HZ);
	PX("\tIn=%lu  Out=%lu" strNL, 10, u32SecToTicks(10));
	PX("\tIn=%lu  Out=%lu" strNL, 10000, u32SecToTicks(10000));
	PX("\tIn=%lu  Out=%lu" strNL, 4200000, u32SecToTicks(4200000));
	// works up to 42000000 @ 100Hz, 4200000 @ 1000Hz, 420000 @ 10000Hz 
#endif
}
