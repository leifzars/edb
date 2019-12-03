/*
 * time_s.h
 *
 */

#ifndef TSD_TIME_TIME_S_H
#define TSD_TIME_TIME_S_H

#include "stdint.h"
#include "env.h"

typedef long long timeTicks_t; //Since 0000-1-1
typedef unsigned long timeSecs_t;  //Since 2000-1-1 max ~>2136
typedef long long timeMonoTicks_t;
typedef long timeMonoSecs_t;

typedef PRE_PACKED struct {
	uint16_t millisecond;
	uint8_t second;
	uint8_t minute;
	uint8_t hour;
	uint8_t day;
	uint8_t month;
	uint16_t year;
	int8_t timeZone; //expressed in quarter of an hour  -47..+48
} POST_PACKED dateTime_t;

#endif /* TSD_TIME_TIME_S_H */
