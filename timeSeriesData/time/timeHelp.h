/*
f * time.h
 *
 *	I am sure their might be a better time library for this.
 *	Based of the .NET DateTime Tick
 *	64 bit int
 *	10,000,000 ticks per second
 *	Zero = 0000-1-1
 */

#ifndef TSD_TIME_TIMEHELP_H
#define TSD_TIME_TIMEHELP_H

#define TIME_ZONE_UTC		0

#define TIME_TICKS_IN_MIC_SEC	10
#define TIME_TICKS_IN_MIL_SEC	10000LL
#define TIME_TICKS_IN_SEC	10000000LL
#define TIME_TICKS_IN_MIN	(10000000LL * 60LL)
#define TIME_TICKS_IN_HR	(10000000LL * (60LL * 60LL))

#define TIME_TICKS_IN_DAY	(TIME_TICKS_IN_HR * 24LL)
#define TIME_TICKS_IN_YEAR	(TIME_TICKS_IN_HR * 24LL)

#define TIME_TICKS_2015_1_1		0x08d1f36d05308000LL
#define TIME_TICKS_2017_1_1		0x08d431d92127c000LL
#define TIME_DAYS_2000_1_1    730119
#define TIME_TICKS_2000_1_1    	0x08c1220247e44000LL
#define TIME_TICKS_1970_1_1		0x089f7ff5f7b58000LL

#define TIME_SEC_TO_TICKS(sec)		((int64_t)(sec) * TIME_TICKS_IN_SEC)
#define TIME_MIL_SEC_TO_TICKS(sec)	((int64_t)sec * TIME_TICKS_IN_MIL_SEC)






#define TIME_ZONE_UTC				0
#define TIME_SECONDS_PER_MINUTE		(60)
#define TIME_SECONDS_PER_HOUR		(60 * 60)
#define TIME_SECONDS_PER_DAY		(60 * 60 * 24)
#define TIME_MILISECONDS_PER_DAY	(1000 * 60 * 60 * 24)


#define SEC_TO_SEC_IN_MIN(sec)	(sec % 60)
#define SEC_TO_MIN_IN_HR(sec)	((sec / 60L) % 60)
#define SEC_TO_HR_IN_DAY(sec)	((sec / 3600L) % 24)
#define SEC_TO_DAYS(sec)		(sec / 86400L)

#define SEC_TO_MILISEC(sec)		((sec) * 1000)
#define DAYS_TO_SEC(days)		(days * 86400L)


#include "calender.h"
#include "time.h"

#include "time_s.h"
#include "period.h"


extern dateTime_t TIME_minDateTime;
extern dateTime_t TIME_maxDateTime;

extern timeTicks_t TIME_minTick;
extern timeTicks_t TIME_maxTick;


timeTicks_t TIME_now();

uint32_t TIME_toString(char* str, timeTicks_t time);


#endif /* TSD_TIME_TIMEHELP_H */
