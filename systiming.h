/*
 * systiming.h
 * Copyright 2014-20 Andre M Maree / KSS Technologies (Pty) Ltd.
 */

#pragma once

#include "hal_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// #################################################################################################

#define	systimerSCATTER				1			// enable scatter graphs support
#define	systimerSCATTER_GROUPS		10			// number of scatter graph groupings

#if		(systimerSCATTER == 1)
	#define	IF_SYSTIMER_INIT(T,n,t,tag, ...)	if (T && ((n) < 31)) vSysTimerInit(n,t,tag,##__VA_ARGS__)
#else
	// Allows macro to take scatter parameters (hence avoid errors if used in macro definition)
	// but discard values passed
	#define	IF_SYSTIMER_INIT(T,n,t,tag, ...)	if (T && ((n) < 31)) vSysTimerInit(n,t,tag)
#endif

#define	IF_SYSTIMER_START(T,n)					if (T && ((n) < 31)) xSysTimerStart(n)
#define	IF_SYSTIMER_STOP(T,n)					if (T && ((n) < 31)) xSysTimerStop(n)
#define	IF_SYSTIMER_TOGGLE(T,n)					if (T && ((n) < 31)) xSysTimerToggle(n)
#define	IF_SYSTIMER_RESET(T,n)					if (T && ((n) < 31)) xSysTimerReset(n)

#define	IF_SYSTIMER_SHOW(T,n)					if (T && ((n) < 31)) vSysTimerShow(n)
#define	IF_SYSTIMER_SHOW_NUM(T,n)				if (T && ((n) < 31)) vSysTimerShow(1 << (n))

// ################################# Process timer support #########################################

enum { stMILLIS, stMICROS, stCLOCKS, stMAX_TYPE } ;

enum {
// ################# SYSTEM TASKS ########################
//	stL2, 		stL3,						// track Lx disconnected time & occurrences
//	stMQTT_RX,	stMQTT_TX,
//	stHTTP,
//	stACT_S0, stACT_S1, stACT_S2, stACT_S3, stACT_SX,
//	stI2Ca,stI2Cb,stI2Cc,stI2Cd,stI2Ce,stI2Cf,stI2Cg,
//	stFOTA,
//	stSLOG,,
//	stTFTP,										// TFTP task execution timing...
//	stRTOS,

// ################### OPTIONAL TASKS ####################
#if	(SW_GUI > 0)
	stGUI0, stGUI1,
#endif

// ####################### DEVICES #######################
#if (halSOC_DIG_IN > 0)
	stGPDINa, stGPDINz = (stGPDINa + halSOC_DIG_IN - 1),
#endif
#if	(halHAS_PCA9555 > 0)
	stPCA9555,
#endif
#if (halHAS_ONEWIRE > 0)
	stOW1, stOW2,
#endif
#if	(halHAS_DS248X > 0)
	stDS248xIO, stDS248x1R, stDS248xWR, stDS248xRD, stDS248xST,
#endif
#if	(halHAS_DS18X20 > 0)
	stDS1820A, stDS1820B,
#endif
#if	(halHAS_DS1990X > 0)
	stDS1990,
#endif
#if	(halHAS_LIS2HH12 > 0)
	stLIS2HH12,
#endif
#if	(halHAS_LTR329ALS > 0)
	stLTR329ALS,
#endif
#if	(halHAS_M90E26 > 0)
	stM90EX6R, stM90EX6W,
#endif
#if	(halHAS_MCP342X > 0)
	stMCP342X,
#endif
#if	(halHAS_MPL3115 > 0)
	stMPL3115,
#endif
#if	(halHAS_PYCOPROC > 0)
	stPYCOPROC,
#endif
#if	(halHAS_SI70XX > 0)
	stSI70XX,
#endif
#if	(halHAS_SSD1306 > 0)
	stSSD1306A, stSSD1306B,
#endif
#if (halHAS_ILI9341 > 0)
	stILI9341a, stILI9341b,
#endif
	stMAX_NUM,				// last in list, define all required above here
	stINVALID = 31,			// maximum timers allowed, beyond here disabled.

// ################# SYSTEM TASKS ########################
	stL2=31, stL3=31,
	stMQTT_RX=31, stMQTT_TX=31,
	stHTTP=31,
	stACT_S0=31, stACT_S1=31, stACT_S2=31, stACT_S3=31, stACT_SX=31,
	stI2Ca=31, stI2Cb=31, stI2Cc=31, stI2Cd=31, stI2Ce=31, stI2Cf=31,stI2Cg=31,
	stFOTA=31,
	stSLOG=31,
	stTFTP=31,
	stRTOS=31,

// ################### OPTIONAL TASKS ####################
#if	(SW_GUI == 0)
	stGUI0=31, stGUI1=31,
#endif

// ####################### DEVICES #######################
#if (halSOC_DIG_IN == 0)
	stGPDINa=31, stGPDINz=31,
#endif
#if	(halHAS_PCA9555 == 0)
	stPCA9555=31,
#endif
#if	(halHAS_ONEWIRE == 0)
	stOW1=31, stOW2=31,
#endif
#if	(halHAS_DS248X == 0)
	stDS248xIO=31, stDS248x1R=31, stDS248xWR=31, stDS248xRD=31, stDS248xST=31,
#endif
#if	(halHAS_DS18X20 == 0)
	stDS1820A=31, stDS1820B=31,
#endif
#if	(halHAS_DS1990X == 0)
	stDS1990=31,
#endif
#if	(halHAS_LIS2HH12 == 0)
	stLIS2HH12=31,
#endif
#if	(halHAS_LTR329ALS == 0)
	stLTR329ALS=31,
#endif
#if	(halHAS_M90E26 == 0)
	stM90EX6R=31,stM90EX6W=31,
#endif
#if	(halHAS_MCP342X == 0)
	stMCP342X=31,
#endif
#if	(halHAS_MPL3115 == 0)
	stMPL3115=31,
#endif
#if	(halHAS_PYCOPROC == 0)
	stPYCOPROC=31,
#endif
#if	(halHAS_SI70XX == 0)
	stSI70XX=31,
#endif
#if	(halHAS_SSD1306 == 0)
	stSSD1306A=31, stSSD1306B=31,
#endif
#if	(halHAS_ILI9341 == 0)
	stILI9341a=31, stILI9341b=31,
#endif
};

// ######################################### Data structures #######################################

typedef struct __attribute__((packed)) systimer_t {
	const char * Tag ;
	uint64_t	Sum ;
	uint32_t	Last, Count, Max, Min ;
#if	defined(ESP_PLATFORM) && !defined(CONFIG_FREERTOS_UNICORE)
	uint32_t	Skip ;
#endif
#if	(systimerSCATTER == 1)
	uint32_t	SGmin, SGmax ;
	uint32_t	Group[systimerSCATTER_GROUPS] ;
#endif
} systimer_t ;

// ######################################### Public variables ######################################


// ######################################### Public functions ######################################

void vSysTimerResetCounters(uint8_t TimNum) ;
void vSysTimerInit(uint8_t TimNum, int Type, const char * Tag, ...) ;
void vSysTimerResetCountersMask(uint32_t TimerMask) ;
uint32_t xSysTimerStart(uint8_t TimNum) ;
uint32_t xSysTimerStop(uint8_t TimNum) ;
uint32_t xSysTimerToggle(uint8_t TimNum);
uint32_t xSysTimerIsRunning(uint8_t TimNum) ;
int	xSysTimerGetStatus(uint8_t TimNum, systimer_t *) ;
uint64_t xSysTimerGetElapsedClocks(uint8_t TimNum) ;
uint64_t xSysTimerGetElapsedMicros(uint8_t TimNum) ;
uint64_t xSysTimerGetElapsedMillis(uint8_t TimNum) ;
uint64_t xSysTimerGetElapsedSecs(uint8_t TimNum) ;
void vSysTimerShow(uint32_t TimerMask) ;
int64_t i64TaskDelayUsec(uint32_t u32Period) ;
uint32_t xClockDelayUsec(uint32_t uSec) ;
uint32_t xClockDelayMsec(uint32_t mSec) ;
void vSysTimingTest(void) ;

#ifdef __cplusplus
}
#endif
