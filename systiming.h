// systiming.h

#pragma once

#include "hal_platform.h"
#include "definitions.h"

#ifdef __cplusplus
extern "C" {
#endif

// #################################################################################################

// Allows both versions of macro to take scatter parameters (avoiding errors)
// but discard values passed unless systimerSCATTER > 2 to enable scatter support
#define	systimerSCATTER							10
#if	(systimerSCATTER > 2)
	#define	IF_SYSTIMER_INIT(T,n,t,tag, ...)	if (T && ((n) < 31)) vSysTimerInit(n,t,tag,##__VA_ARGS__)
#else
	#define	IF_SYSTIMER_INIT(T,n,t,tag, ...)	if (T && ((n) < 31)) vSysTimerInit(n,t,tag)
#endif
#define	IF_SYSTIMER_DEINIT(T,n)					if (T && ((n) < 31)) xSysTimerDeInit(n)
#define	IF_SYSTIMER_START(T,n)					if (T && ((n) < 31)) xSysTimerStart(n)
#define	IF_SYSTIMER_STOP(T,n)					if (T && ((n) < 31)) xSysTimerStop(n)
#define	IF_SYSTIMER_TOGGLE(T,n)					if (T && ((n) < 31)) xSysTimerToggle(n)
#define	IF_SYSTIMER_RESET(T,n)					if (T && ((n) < 31)) xSysTimerReset(n)
#define	IF_SYSTIMER_SHOW(T,n)					if (T && ((n) < 31)) vSysTimerShow(NULL, n)
#define	IF_SYSTIMER_SHOW_NUM(T,n)				if (T && ((n) < 31)) vSysTimerShow(NULL, 1 << (n))

// ################################# Process timer support #########################################

enum { stMILLIS, stMICROS, stCLOCKS, stMAX_TYPE };

enum {
// ################# SYSTEM TASKS ########################
//	stMQTT_RX, stMQTT_TX,
//	stHTTP,
	stACT_S0, stACT_S1, stACT_S2, stACT_S3, stACT_SX,
	stI2Ca, stI2Cb, stI2Cc,
//	stFOTA,
//	stTFTP,							// TFTP task execution timing...
//	stRTOS,
#if (appGUI > 0)
	stGUI0, stGUI1,
#endif
// ####################### DEVICES #######################
#if	(HAL_ADE7953 > 0)
	stADE7953R, stADE7953W,
#endif
#if	(HAL_PCA9555 > 0)
	stPCA9555,
#endif
#if (HAL_ONEWIRE > 0)
	stOW1, stOW2,
#endif
#if	(HAL_DS1307 > 0)
	stDS1307,
#endif
#if	(HAL_DS248X > 0)
	stDS248x,
#endif
#if	(HAL_DS18X20 > 0)
	stDS1820A, stDS1820B,
#endif
#if	(HAL_DS1990X > 0)
	stDS1990,
#endif
#if	(HAL_LIS2HH12 > 0)
	stLIS2HH12,
#endif
#if	(HAL_LTR329ALS > 0)
	stLTR329ALS,
#endif
#if	(HAL_M90E26 > 0)
	stM90EX6R, stM90EX6W,
#endif
#if	(HAL_MCP342X > 0)
	stMCP342X,
#endif
#if	(HAL_MPL3115 > 0)
	stMPL3115,
#endif
#if	(HAL_PYCOPROC > 0)
	stPYCOPROC,
#endif
#if	(HAL_PCF8574 > 0)
	stPCF8574,
#endif
#if	(HAL_SI70XX > 0)
	stSI70XX,
#endif
#if	(HAL_SSD1306 > 0)
	stSSD1306A, stSSD1306B,
#endif
#if (HAL_WS281X > 0)
	stWS281X,
#endif
#if (HAL_ILI9341 > 0)
	stILI9341a, stILI9341b,
#endif
	stMAX_NUM,				// last in list, define all required above here
	stINVALID = 31,			// maximum timers allowed, beyond here disabled.

// ################# SYSTEM TASKS ########################
	stMQTT_RX=31, stMQTT_TX=31,
	stHTTP=31,
//	stACT_S0=31, stACT_S1=31, stACT_S2=31, stACT_S3=31, stACT_SX=31,
//	stI2Ca=31, stI2Cb=31, stI2Cc=31,
	stFOTA=31,
	stTFTP=31,
	stRTOS=31,
#if (appGUI == 0)
	stGUI0=31, stGUI1=31,
#endif
// ####################### DEVICES #######################
#if	(HAL_ADE7953 == 0)
	stADE7953R=31,stADE7953W=31,
#endif
#if	(HAL_PCA9555 == 0)
	stPCA9555=31,
#endif
#if	(HAL_ONEWIRE == 0)
	stOW1=31, stOW2=31,
#endif
#if	(HAL_DS1307 == 0)
	stDS1307=31,
#endif
#if	(HAL_DS248X == 0)
	stDS248xIO=31,
#endif
#if	(HAL_DS18X20 == 0)
	stDS1820A=31, stDS1820B=31,
#endif
#if	(HAL_DS1990X == 0)
	stDS1990=31,
#endif
#if	(HAL_LIS2HH12 == 0)
	stLIS2HH12=31,
#endif
#if	(HAL_LTR329ALS == 0)
	stLTR329ALS=31,
#endif
#if	(HAL_M90E26 == 0)
	stM90EX6R=31,stM90EX6W=31,
#endif
#if	(HAL_MCP342X == 0)
	stMCP342X=31,
#endif
#if	(HAL_MPL3115 == 0)
	stMPL3115=31,
#endif
#if	(HAL_PYCOPROC == 0)
	stPYCOPROC=31,
#endif
#if	(HAL_PCF8574 == 0)
	stPCF8574=31,
#endif
#if	(HAL_SI70XX == 0)
	stSI70XX=31,
#endif
#if	(HAL_SSD1306 == 0)
	stSSD1306A=31, stSSD1306B=31,
#endif
#if (HAL_WS281X == 0)
	stWS281X = 31,
#endif
#if	(HAL_ILI9341 == 0)
	stILI9341a=31, stILI9341b=31,
#endif
};

// ######################################### Data structures #######################################

typedef struct __attribute__((packed)) {
	u32_t Count, Last, Min, Max;
	u64_t Sum;
	const char * Tag;
	#if	(systimerSCATTER > 2)
		u32_t SGmin, SGmax, Group[systimerSCATTER];
		#define stSCATTER_OVERHEAD		((systimerSCATTER + 2) * sizeof(u32_t))
	#else
		#define stSCATTER_OVERHEAD		0
	#endif
	#ifndef CONFIG_FREERTOS_UNICORE
		u32_t Skip;
		#define stDUALCORE_OVERHEAD		sizeof(u32_t)
	#else
		#define stDUALCORE_OVERHEAD		0
	#endif
} systimer_t;
DUMB_STATIC_ASSERT(sizeof(systimer_t) == 24 + sizeof(char *) + stSCATTER_OVERHEAD + stDUALCORE_OVERHEAD);

// ######################################### Public variables ######################################

// ################################### Public Control APIs #########################################

void vSysTimerInit(u8_t TimNum, int Type, const char * Tag, ...);

void vSysTimerDeInit(u8_t TimNum);

/**
 * @brief	start the specified timer
 * @param 	TimNum
 * @return	current timer value based on type (CLOCKs or TICKs)
 */
u32_t xSysTimerStart(u8_t TimNum);

/**
 * @brief	stop the specified timer and update the statistics
 * @param	TimNum
 * @return	Last measured interval based on type (CLOCKs or TICKs)
 */
u32_t xSysTimerStop(u8_t TimNum);

u32_t xSysTimerToggle(u8_t TimNum);

/**
 * @brief	Reset all the timer values for 1 or more timers
 * @brief 	This function does NOT reset SGmin & SGmax. To reset Min/Max use vSysTimerInit()
 * @brief	which allows for the type to be changed as well as specifying new Min/Max values.
 * @param	TimerMask
 */
void vSysTimerResetCountersMask(u32_t TimerMask);

// ################################### Public Status APIs ##########################################

/**
 * @brief	Check if timer is running
 * @param	TimNum
 * @return	0 if not running else current elapsed timer value based on type (CLOCKs or TICKSs)
 */
u32_t xSysTimerIsRunning(u8_t TimNum);

/**
 * @brief	return the current timer configuration and status
 * @param[in]	TimNum timer number
 * @param[out]	pST pointer to structure to be filled
 * @return		timer type
 */
int	xSysTimerGetStatus(u8_t TimNum, systimer_t *);

// #################################### Elapsed time APIs ##########################################

u64_t xSysTimerGetElapsedClocks(u8_t TimNum);

u64_t xSysTimerGetElapsedMicros(u8_t TimNum);

u64_t xSysTimerGetElapsedMillis(u8_t TimNum);

u64_t xSysTimerGetElapsedSecs(u8_t TimNum);

// ################################## Timer status reporting #######################################

struct report_t;
/**
 * @brief	display the current value(s) of the specified timer(s)
 * @brief	MUST do a SysTimerStop() before calling to freeze accurate value in array
 * @param	tMask 8bit bitmapped flag to select timer(s) to display
 * @return	none
 */
void vSysTimerShow(struct report_t * psR, u32_t TimerMask);

// ################################### RTOS + HW delay support #####################################

/**
 * @brief	delay by yielding program execution for a specified number of uSecs
 * @param	u32Period of uSecs to delay
 * @return	Clock counter at the end
 */
i64_t i64TaskDelayUsec(u32_t u32Period);

// ################################## MCU Clock cycle delay support ################################

/**
 * @brief	delay (not yielding) program execution for a specified number of uSecs
 * @param	Number of uSecs to delay
 */
void vClockDelayUsec(u32_t uSec);

/**
 * @brief	delay (not yielding) program execution for a specified number of mSecs
 * @param	Number of mSecs to delay
 */
void vClockDelayMsec(u32_t mSec);

// ##################################### functional tests ##########################################

void vSysTimingTest(void);

#ifdef __cplusplus
}
#endif
