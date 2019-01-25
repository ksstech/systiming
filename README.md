Description:
============

A low overhead but very accurate timing module to measure elapsed time in RTOS ticks or MCU clocks. Primarily of use to time the execution of specific functions, operations or subsets of code. Can be stopped, started and reset as required. Keeps track basic statistics such as the minimum, maximum, sum and count of times, calculate the average. Also support [optional] statistical analysis using a configureable number of scatter buckkets to track timing spread.


Features:
=========
Support RTOS tick (mSec) and MCU clock (uSec) based timing.
Specifically coded to minimise overhead during stop/start/reeet operation making the actual measurement very accurate.
Compile time option to include scatter group functionality
Compile time option to specify the number of scatter groups
	Optional Scatter Groups functionality

Value		STmin		STmax	G-0		G-1		G-2		G-3		G-4		G-5		G-6		G-7		G-8		G-9
			100			900
1								x
100								x			
101										x

899																								x
900																										x
999																										x
999999																									x


Comparison:
===========

Feature					TICKtimer							CLOCKtimer						Comments
-------					---------							----------						--------
Timesource				RTOS tick							MCU clock counter
Resolution				Based on tick frequency				Based on clock speed  
	Default				1KHz = 1mSec						80MHz=12.5nSec, 250Mhz=4.0nS
Display Unit			mSec								uSec & clocks

Statistics available	Min, Max, Avg, Count, Sum			Min, Max, Avg, Count, Sum
Scatter groups (SG)		Compile time option					Compile time option
Default #SG buckets		10 (compile time selected)			10 (compile time selected)

Comments:
=========
