/*
 * systiming.h
 * Copyright 2014-20 Andre M Maree / KSS Technologies (Pty) Ltd.
 */

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
	#define	IF_SYSTIMER_INIT(T,n,t,tag, ...)	if (T) vSysTimerInit(n, t, tag, ##__VA_ARGS__)
#else
	// Allows macro to take scatter parameters (hence avoid errors if used in macro definition)
	// but discard values passed
	#define	IF_SYSTIMER_INIT(T,n,t,tag, ...)	if (T) vSysTimerInit(n,t,tag)
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
	systimerOW1, systimerOW2,
	systimerDS248xA, systimerDS248xB, systimerDS248xC, systimerDS248xD,  systimerDS248xE,  systimerDS248xF,
	systimerDS1820A,systimerDS1820B,
	systimerDS1990,
//	systimerM90EX6,
	systimerACT_S0, systimerACT_S1, systimerACT_S2, systimerACT_S3, systimerACT_SX,
	systimerSSD1306A, systimerSSD1306B,
	systimerILI9341_1, systimerILI9341_2,
//	systimerTFTP,										// TFTP task execution timing...
	systimerGUI0, systimerGUI1, systimerGUI2,
	systimerMAX_NUM,									// last in list, define all required above here

// From here we list disabled timers to avoid compile errors
//	systimerMQTT_RX = 31,	systimerMQTT_TX = 31,
	systimerHTTP = 31,
	systimerFOTA = 31,
	systimerSLOG = 31,
//	systimerPCA9555 = 31,
//	systimerOW1 = 31, systimerOW2 = 31,
//	systimerDS248xA = 31, systimerDS248xB = 31, systimerDS248xC = 31, systimerDS248xD = 31, systimerDS248xE = 31, systimerDS248xF = 31,
//	systimerDS1820A = 31, systimerDS1820B = 31,
//	systimerDS1990 = 31,
	systimerM90EX6 = 31,
//	systimerACT_S0 = 31, systimerACT_S1 = 31, systimerACT_S2 = 31, systimerACT_S3 = 31, systimerACT_SX = 31,
//	systimerSSD1306A = 31, systimerSSD1306B = 31,
//	systimerILI9341_1 = 31, systimerili9341_2 = 31,
	systimerTFTP = 31,
//	systimerGUI0 = 31, systimerGUI1 = 31, systimerGUI2 = 31,
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
