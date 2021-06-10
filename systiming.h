/*
 * systiming.h
 * Copyright 2014-20 Andre M Maree / KSS Technologies (Pty) Ltd.
 */

#pragma once

#include	"hal_config.h"

#include	<stdbool.h>
#include	<stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// #################################################################################################

#define	systimerSCATTER				1			// enable scatter graphs support
#define	systimerSCATTER_GROUPS		10			// number of scatter graph groupings
#define	systimerSCATTER_CLOCKS		0

#if		(systimerSCATTER == 1)
	#define	IF_SYSTIMER_INIT(T,n,t,tag, ...)	if (T && (n<32)) vSysTimerInit(n, t, tag, ##__VA_ARGS__)
#else
	// Allows macro to take scatter parameters (hence avoid errors if used in macro definition)
	// but discard values passed
	#define	IF_SYSTIMER_INIT(T,n,t,tag, ...)	if (T && (n<32)) vSysTimerInit(n,t,tag)
#endif

#define	IF_SYSTIMER_START(T,n)					if (T && (n<32)) xSysTimerStart(n)
#define	IF_SYSTIMER_STOP(T,n)					if (T && (n<32)) xSysTimerStop(n)
#define	IF_SYSTIMER_RESET(T,n)					if (T && (n<32)) xSysTimerReset(n)

#define	IF_SYSTIMER_SHOW(T,n)					if (T && (n<32)) vSysTimerShow(n)
#define	IF_SYSTIMER_SHOW_NUM(T,n)				if (T && (n<32)) vSysTimerShow(1 << n)

// ################################# Process timer support #########################################

enum { systimerTICKS, systimerCLOCKS } ;

enum {
// ################# SYSTEM TASKS ########################
//	systimerL2, 		systimerL3,						// track Lx disconnected time & occurrences
//	systimerMQTT_RX,	systimerMQTT_TX,
//	systimerHTTP,
//	systimerACT_S0, systimerACT_S1, systimerACT_S2, systimerACT_S3, systimerACT_SX,
	systimerI2Ca,systimerI2Cb,systimerI2Cc,systimerI2Cd,systimerI2Ce,systimerI2Cf,
//	systimerFOTA,
//	systimerSLOG,
//	systimerTFTP,										// TFTP task execution timing...
	systimerRTOS,
// ################### OPTIONAL TASKS ####################
#if	(SW_GUI > 0)
	systimerGUI0, systimerGUI1, systimerGUI2,
#endif
// ####################### DEVICES #######################
#if	(halHAS_PCA9555 > 0)
	systimerPCA9555,
#endif
#if (halHAS_ONEWIRE > 0)
	systimerOW1, systimerOW2,
#endif
#if	(halHAS_DS248X > 0)
	systimerDS248xA, systimerDS248xB, systimerDS248xC, systimerDS248xD, systimerDS248xE, systimerDS248xF,
#endif
#if	(halHAS_DS18X20 > 0)
	systimerDS1820A,systimerDS1820B,
#endif
#if	(halHAS_DS1990X > 0)
	systimerDS1990,
#endif
#if	(halHAS_M90E26 > 0)
	systimerM90EX6,
#endif
#if	(halHAS_MCP342X > 0)
	systimerMCP342X,
#endif
#if	(halHAS_SSD1306 > 0)
	systimerSSD1306A, systimerSSD1306B,
#endif
#if (halHAS_ILI9341 > 0)
	systimerILI9341a, systimerILI9341b,
#endif
	systimerMAX_NUM,				// last in list, define all required above here
	systimerINVALID = 32,			// maximum timers allowed, beyond here disabled.
// ################# SYSTEM TASKS ########################
	systimerL2, systimerL3,
	systimerMQTT_RX, systimerMQTT_TX,
	systimerHTTP,
//	systimerI2Ca,systimerI2Cb,systimerI2Cc,systimerI2Cd,systimerI2Ce,systimerI2Cf,
	systimerACT_S0, systimerACT_S1, systimerACT_S2, systimerACT_S3, systimerACT_SX,
	systimerFOTA,
//	systimerSLOG,
//	systimerTFTP,
//	systimerRTOS,

// ################### OPTIONAL TASKS ####################
#if	(SW_GUI == 0)
	systimerGUI0, systimerGUI1, systimerGUI2,
#endif
// ####################### DEVICES #######################
#if	(halHAS_PCA9555 == 0)
	systimerPCA9555,
#endif
#if	(halHAS_ONEWIRE == 0)
	systimerOW1, systimerOW2,
#endif
#if	(halHAS_DS248X == 0)
	systimerDS248xA, systimerDS248xB, systimerDS248xC, systimerDS248xD, systimerDS248xE, systimerDS248xF,
#endif
#if	(halHAS_DS18X20 == 0)
	systimerDS1820A, systimerDS1820B,
#endif
#if	(halHAS_DS1990X == 0)
	systimerDS1990,
#endif
#if	(halHAS_M90E26 == 0)
	systimerM90EX6,
#endif
#if	(halHAS_MCP342X == 0)
	systimerMCP342X,
#endif
#if	(halHAS_SSD1306 == 0)
	systimerSSD1306A, systimerSSD1306B,
#endif
#if	(halHAS_ILI9341 == 0)
	systimerILI9341a, systimerILI9341b,
#endif
} ;

// ######################################### Data structures #######################################

typedef struct __attribute__((packed)) systimer_t {
	const char * Tag ;
	uint64_t	Sum ;
	uint32_t	Last, Count, Min, Max ;
#if		(systimerSCATTER == 1)
	uint32_t	SGmin, SGmax ;
	uint32_t	Group[systimerSCATTER_GROUPS] ;
#endif
} systimer_t ;

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

int64_t i64TaskDelayUsec(uint32_t u32Period) ;

uint32_t xClockDelayUsec(uint32_t uSec) ;
uint32_t xClockDelayMsec(uint32_t mSec) ;

void	vSysTimingTest(void) ;

#ifdef __cplusplus
}
#endif
