// systiming.c - Copyright (c) 2014-24 Andre M. Maree / KSS Technologies (Pty) Ltd.

#include <string.h>

#include "systiming.h"
#include "hal_memory.h"
#include "struct_union.h"
#include "syslog.h"
#include "FreeRTOS_Support.h"

#include "xtensa/hal.h"
#include "esp_timer.h"

#define	debugFLAG					0xF000
#define	debugINIT					(debugFLAG & 0x0001)

#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// ########################################## Macros ###############################################

#define	SetTT(i,x)					maskSET2B(STtype,i,x,u64_t)
#define	GetTT(i)					maskGET2B(STtype,i,u64_t)
#define GetTimer(t) ((t == stCLOCKS) ? xthal_get_ccount() : \
					(t == stMICROS) ? esp_timer_get_time() : \
					xTaskGetTickCount())

// #################################### Local static variables #####################################

static systimer_t	STdata[stMAX_NUM] = { 0 };
static u32_t STstat = 0;								// 1 = Running
static u64_t STtype = 0;

#ifndef CONFIG_FREERTOS_UNICORE
	static u32_t STcore = 0;							// Core# 0/1
#endif

void vSysTimerResetCounter(u8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < stMAX_NUM);
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

void vSysTimerResetCountersMask(u32_t TimerMask) {
	u32_t mask = 0x00000001;
	systimer_t *pST	= STdata;
	for (u8_t TimNum = 0; TimNum < stMAX_NUM; ++TimNum, ++pST) {
		if (TimerMask & mask) vSysTimerResetCounters(TimNum);
		mask <<= 1;
	}
}

void vSysTimerInit(u8_t TimNum, int Type, const char * Tag, ...) {
	IF_myASSERT(debugPARAM, (TimNum < stMAX_NUM) && (Type < stMAX_TYPE));
	systimer_t *pST	= &STdata[TimNum];
	pST->Tag = Tag;
	SetTT(TimNum, Type);
	vSysTimerResetCounter(TimNum);
	#if	(systimerSCATTER > 2)
    	va_list vaList;
    	va_start(vaList, Tag);
    	// Assume default type is stMICROS so values in uSec
		pST->SGmin	= va_arg(vaList, u32_t);
		pST->SGmax	= va_arg(vaList, u32_t);
		IF_myASSERT(debugPARAM, pST->SGmin < pST->SGmax);
		// if stMILLIS handle uSec to Ticks conversion
		if (Type == stMILLIS) {
			#if (CONFIG_FREERTOS_HZ < MILLIS_IN_SECOND)
				pST->SGmin /= (MILLIS_IN_SECOND / CONFIG_FREERTOS_HZ);
				pST->SGmax /= (MILLIS_IN_SECOND / CONFIG_FREERTOS_HZ);
				#elif (CONFIG_FREERTOS_HZ > MILLIS_IN_SECOND)
				pST->SGmin *= (CONFIG_FREERTOS_HZ / MILLIS_IN_SECOND);
				pST->SGmax *= (CONFIG_FREERTOS_HZ / MILLIS_IN_SECOND);
			#endif
		}
		va_end(vaList);
	#endif
}

u32_t xSysTimerStart(u8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < stMAX_NUM);
	int Type = GetTT(TimNum);
	IF_myASSERT(debugPARAM, Type < stMAX_TYPE);
	#ifndef CONFIG_FREERTOS_UNICORE
	if (Type == stCLOCKS) {
		if (xPortGetCoreID()) STcore |= (1UL<<TimNum);	// Running on Core 1
		else STcore &= ~(1UL << TimNum);				// Running on Core 0
	}
	#endif
	STstat |= (1UL << TimNum);							// Mark as started & running
	++STdata[TimNum].Count;
	return STdata[TimNum].Last = GetTimer(Type);
}

u32_t xSysTimerStop(u8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < stMAX_NUM);
	int Type = GetTT(TimNum);
	IF_myASSERT(debugPARAM, Type < stMAX_TYPE);
	u32_t tNow	= GetTimer(Type);
	STstat &= ~(1UL << TimNum);
	systimer_t *pST	= &STdata[TimNum];

	#ifndef CONFIG_FREERTOS_UNICORE
	/* Adjustments made to CCOUNT cause discrepancies between readings from different cores.
	 * In order to filter out invalid/OOR values we verify whether the timer is being stopped
	 * on the same MCU as it was started. If not, we ignore the timing values */
	u8_t	xCoreID	= (STcore & (1UL << TimNum)) ? 1 : 0;
	if ((Type == stCLOCKS) && xCoreID != xPortGetCoreID()) {
		++pST->Skip;
		return 0;
	}
	#endif
	u32_t tElap = tNow - pST->Last;
	pST->Sum += tElap;
	pST->Last = tElap;
	// update Min & Max if required
	if (pST->Min > tElap) pST->Min = tElap;
	if (pST->Max < tElap) pST->Max = tElap;
	#if	(systimerSCATTER > 2)
	int Idx;
	if (tElap <= pST->SGmin)		Idx = 0;
	else if (tElap >= pST->SGmax)	Idx = systimerSCATTER-1;
	else 							Idx = 1 + ((tElap-pST->SGmin)*(systimerSCATTER-2)) / (pST->SGmax-pST->SGmin);
	++pST->Group[Idx];
	IF_PX(debugRESULT && OUTSIDE(0, Idx, systimerSCATTER-1), "l=%lu h=%lu n=%lu i=%d" strNL,
			pST->SGmin, pST->SGmax, tElap, Idx);
	IF_myASSERT(debugRESULT, INRANGE(0, Idx, systimerSCATTER-1));
	#endif
	return tElap;
}

u32_t xSysTimerToggle(u8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < stMAX_NUM);
	return (STstat & (1 << TimNum)) ? xSysTimerStop(TimNum) : xSysTimerStart(TimNum);
}

u32_t xSysTimerIsRunning(u8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < stMAX_NUM);
	u32_t tNow = 0;
	if (STstat & (1 << TimNum)) {
		int Type = GetTT(TimNum);
		IF_myASSERT(debugPARAM, Type < stMAX_TYPE);
		tNow	= GetTimer(Type);
		systimer_t * pST = &STdata[TimNum];
		if (Type == stCLOCKS) {
			if (tNow > pST->Last) tNow -= pST->Last;	// Unlikely wrapped
			else tNow += (0xFFFFFFFF - pST->Last);		// definitely wrapped
		} else {
			tNow -= pST->Last;
		}
	}
	return tNow;
}

int	xSysTimerGetStatus(u8_t TimNum, systimer_t * pST) {
	IF_myASSERT(debugPARAM, TimNum < stMAX_NUM && halMemorySRAM((void*) pST));
	memcpy(pST, &STdata[TimNum], sizeof(systimer_t));
	return GetTT(TimNum);
}

u64_t xSysTimerGetElapsedClocks(u8_t TimNum) {
	IF_myASSERT(debugPARAM, (TimNum < stMAX_NUM) && GetTT(TimNum) > stMICROS);
	return STdata[TimNum].Sum;
}

u64_t xSysTimerGetElapsedMicros(u8_t TimNum) {
	IF_myASSERT(debugPARAM, (TimNum < stMAX_NUM) && GetTT(TimNum) > stMILLIS);
	return (GetTT(TimNum) == stMICROS) ? STdata[TimNum].Sum : CLOCKS2US(STdata[TimNum].Sum, u64_t);
}

u64_t xSysTimerGetElapsedMillis(u8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < stMAX_NUM);
	int Type = GetTT(TimNum);
	IF_myASSERT(debugPARAM, Type < stMAX_TYPE);
	return (Type == stMILLIS) ? TICK2MS(STdata[TimNum].Sum, u64_t)
		 : (Type == stMICROS) ? MICRO2MS(STdata[TimNum].Sum, u64_t)
		 : CLOCK2MS(STdata[TimNum].Sum, u64_t);
}

u64_t xSysTimerGetElapsedSecs(u8_t TimNum) {
	IF_myASSERT(debugPARAM, TimNum < stMAX_NUM);
	int Type = GetTT(TimNum);
	IF_myASSERT(debugPARAM, Type < stMAX_TYPE);
	return (Type == stMILLIS) ? TICK2SEC(STdata[TimNum].Sum, u64_t)
		 : (Type == stMICROS) ? MICRO2SEC(STdata[TimNum].Sum, u64_t)
		 : CLOCK2SEC(STdata[TimNum].Sum, u64_t);
}

#define	stHDR_TICKS		" mS|Min mS |Max mS |Avg mS |Sum mS |"
#define	stHDR_MICROS	" uS|Min uS |Max uS |Avg uS |Sum uS |"
#define	stHDR_CLOCKS	"Clk|Min Clk|Max Clk|Avg Clk|Sum Clk|"
#define stHDR_FMT1		"%C| # |  Name  | Count |Last%s%C"
#define stHDR_FMT2		"X-MCU-Y|"
#define stDTL_FMT1		"|%2d%c|%8s|%#'7lu|"
#define stDTL_FMT2		"%#'7lu|%#'7lu|%#'7lu|%#'7lu|%#'7llu|"

void vSysTimerShow(report_t * psR, u32_t TimerMask) {
	const char * pcTag;
	char caTmp[12];
	for (int Type = 0; Type < stMAX_TYPE; ++Type) {
		u32_t Mask = 0x00000001;
		int HdrDone = 0;
		for (int Num = 0; Num < stMAX_NUM; Mask <<= 1, ++Num) {
			systimer_t * pST = &STdata[Num];
			if ((TimerMask & Mask) && (Type == GetTT(Num)) && pST->Count) {
				if (HdrDone == 0) {
					xReport(psR, stHDR_FMT1, xpfSGR(0,0,colourFG_CYAN,0),
						(Type == stMILLIS) ? stHDR_TICKS :
						(Type == stMICROS) ? stHDR_MICROS : stHDR_CLOCKS,
						xpfSGR(0,0,attrRESET,0));
				#ifndef CONFIG_FREERTOS_UNICORE
					if (Type == stCLOCKS)
						xReport(psR, stHDR_FMT2);
				#endif
					xReport(psR, strNL);
					HdrDone = 1;
				}
				if (halMemoryANY((void *)pST->Tag)) {
					pcTag = pST->Tag;
				} else {
					snprintfx(caTmp, sizeof(caTmp), "T#%d+%d", pST->Tag, Num - (int)pST->Tag);
					pcTag = caTmp;
				}
				xReport(psR, stDTL_FMT1, Num, (STstat & (1UL << Num)) ? 'R' : ' ', pcTag, pST->Count);
				xReport(psR, stDTL_FMT2, pST->Last, pST->Min, pST->Max,(u32_t) (pST->Count ? (pST->Sum / pST->Count) : pST->Sum), pST->Sum);
				#ifndef CONFIG_FREERTOS_UNICORE
					if (Type == stCLOCKS)
						xReport(psR, "%#'7lu|", pST->Skip);
				#endif
				#if	(systimerSCATTER > 2)
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

u32_t xClockDelayUsec(u32_t uSec) {
	IF_myASSERT(debugPARAM, uSec < (UINT32_MAX / configCLOCKS_PER_USEC));
	u32_t ClockEnd	= GET_CLOCK_COUNTER() + halUS_TO_CLOCKS(uSec);
	while ((ClockEnd - GET_CLOCK_COUNTER()) > configCLOCKS_PER_USEC );
	return ClockEnd;
}

u32_t xClockDelayMsec(u32_t mSec) {
	IF_myASSERT(debugPARAM, mSec < (UINT32_MAX / configCLOCKS_PER_MSEC));
	return xClockDelayUsec(mSec * MICROS_IN_MILLISEC);
}

// ##################################### functional tests ##########################################

#define	systimerTEST_DELAY			1
#define	systimerTEST_TICKS			1
#define	systimerTEST_CLOCKS			1

void vSysTimingTestSet(u32_t Type, char * Tag, u32_t Delay) {
	for (u8_t Idx = 0; Idx < stMAX_NUM; ++Idx)
		vSysTimerInit(Idx, Type, Tag, myMS_TO_TICKS(Delay), myMS_TO_TICKS(Delay * systimerSCATTER));
	for (u32_t Steps = 0; Steps <= systimerSCATTER; ++Steps) {
		for (u32_t Count = 0; Count < stMAX_NUM; xSysTimerStart(Count++));
		vTaskDelay(pdMS_TO_TICKS((Delay * Steps) + 1));
		for (u32_t Count = 0; Count < stMAX_NUM; xSysTimerStop(Count++));
	}
	vSysTimerShow(NULL, 0xFFFFFFFF);
}

void vSysTimingTest(void) {
	#if	(systimerTEST_DELAY == 1)						// Test the uSec delays
	u32_t	uClock, uSecs;
	uClock	= xthal_get_ccount();
	uSecs	= xClockDelayUsec(100);
	PX("Delay=%'lu uS" strNL, (uSecs - uClock) / configCLOCKS_PER_USEC);

	uClock	= xthal_get_ccount();
	uSecs	= xClockDelayUsec(1000);
	PX("Delay=%'lu uS" strNL, (uSecs - uClock) / configCLOCKS_PER_USEC);

	uClock	= xthal_get_ccount();
	uSecs	= xClockDelayUsec(10000);
	PX("Delay=%'lu uS" strNL, (uSecs - uClock) / configCLOCKS_PER_USEC);
	#endif
	#if (systimerTEST_TICKS == 1)						// Test TICK timers & Scatter groups
	vSysTimingTestSet(stMILLIS, "TICKS", 1);
	vSysTimingTestSet(stMILLIS, "TICKS", 10);
	vSysTimingTestSet(stMILLIS, "TICKS", 100);
	vSysTimingTestSet(stMILLIS, "TICKS", 1000);
	#endif
	#if (stTEST_CLOCKS == 1)						// Test CLOCK timers & Scatter groups
	vSysTimingTestSet(stCLOCKS, "CLOCKS", 1);
	vSysTimingTestSet(stCLOCKS, "CLOCKS", 10);
	vSysTimingTestSet(stCLOCKS, "CLOCKS", 100);
	vSysTimingTestSet(stCLOCKS, "CLOCKS", 1000);
	#endif
}
