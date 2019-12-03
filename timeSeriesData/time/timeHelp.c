/*
* time.c
*
*/

#include "string.h"
#include "timeHelp.h"
#include "calender.h"

#define DAYS_IN_MONTH(month)		(daysPerMonth[month - 1])
static const uint8_t daysPerMonth[] = { //
		31, /* Jan */
		28, /* Feb */
		31, /* March */
		30, /* April */
		31, /* May */
		30, /* June */
		31, /* July */
		31, /* Aug */
		30, /* Sep */
		31, /* Oct */
		30, /* Nov */
		31, /* Dec */
		};

dateTime_t TIME_minDateTime = { 0, 0, 0, 0, 1, 1, 0, 0 };
dateTime_t TIME_maxDateTime = { 999, 59, 59, 23, 31, 12, 255, 0 };

timeTicks_t TIME_minTick = 0;
timeTicks_t TIME_maxTick =  3155378975999999999;

timeTicks_t TIME_now(){
	   time_t curtime;

	   time(&curtime);//localtime

	   timeTicks_t t;
	   t = TIME_TICKS_1970_1_1 + curtime * TIME_TICKS_IN_SEC + clock()* TIME_TICKS_IN_MIL_SEC;
	   return t;
}
//
//
//
//bool TIME_isValid(dateTime_t* dt) {
//	if (dt->month < MONTH_JANUARY || dt->month > MONTH_DECEMBER)
//		return false;
//
//	uint8_t leapYearAdj = 0;
//	if ((dt->month == MONTH_FEBRUARY) && IS_LEAP_YEAR(dt->year))
//		leapYearAdj = 1;
//	if (dt->day < 1 || dt->day > (DAYS_IN_MONTH(dt->month) + leapYearAdj))
//		return false;
//
//	if (dt->hour > 23)
//		return false;
//	if (dt->minute > 60)
//		return false;
//	if (dt->second > 60)
//		return false;
//	if (dt->millisecond > 999)
//		return false;
//	if (dt->timeZone < -47 || dt->timeZone > +48)
//		return false;
//
//	return true;
//}
//

void TIME_toDateTimeStruct(timeTicks_t ticks, dateTime_t* out_dt) {
	int64_t tmp;
	bool isLeapYear;
//uint32_t cyclCtStart, cyclCtEnd, cyclCtDiff;
//DWT_BASE_PTR->c

	memcpy(out_dt, &TIME_minDateTime, sizeof(dateTime_t));

	if (ticks >= TIME_TICKS_2015_1_1) {
		out_dt->year = 15;
		ticks -= TIME_TICKS_2015_1_1;
	} else if (ticks >= TIME_TICKS_2000_1_1) {
		out_dt->year = 0;
		ticks -= TIME_TICKS_2000_1_1;
	}

	do {
		if (IS_LEAP_YEAR(out_dt->year)) {
			tmp = (int64_t)DAYS_IN_LEAP_YEAR * TIME_TICKS_IN_DAY;
		} else {
			tmp = (int64_t)DAYS_IN_YEAR * TIME_TICKS_IN_DAY;
		}

		if (ticks < tmp) {
			break;
		}
		ticks -= tmp;
		out_dt->year++;
	} while (1);

	isLeapYear = IS_LEAP_YEAR(out_dt->year);
	do {
		tmp = DAYS_IN_MONTH(out_dt->month);
		if (isLeapYear && out_dt->month == MONTH_FEBRUARY)
			tmp++;

		tmp *= TIME_TICKS_IN_DAY;

		if (ticks < tmp) {
			break;
		}
		ticks -= tmp;
		out_dt->month++;
	} while (1);

	out_dt->day += ticks / TIME_TICKS_IN_DAY;
	ticks = ticks % TIME_TICKS_IN_DAY;
	out_dt->hour = ticks / TIME_TICKS_IN_HR;
	ticks = ticks % TIME_TICKS_IN_HR;

	out_dt->minute = ticks / (uint32_t)TIME_TICKS_IN_MIN;
	ticks = ticks % (uint32_t)TIME_TICKS_IN_MIN;
	out_dt->second = (uint32_t)ticks / (uint32_t)TIME_TICKS_IN_SEC;
	ticks = (uint32_t)ticks % (uint32_t)TIME_TICKS_IN_SEC;

	out_dt->millisecond = (uint32_t)ticks / (uint32_t)TIME_TICKS_IN_MIL_SEC;
	//ticks = ticks % TIME_TICKS_IN_MIL_SEC;
}

uint32_t TIME_toStringDT(char* str, dateTime_t *datetime) {
	return sprintf(str, "%d/%d/%d %02d:%02d:%02d", datetime->month, datetime->day, datetime->year,
			datetime->hour, datetime->minute, datetime->second);
}
uint32_t TIME_toString(char* str, timeTicks_t time) {
	dateTime_t datetime;
	TIME_toDateTimeStruct(time, &datetime);
	return TIME_toStringDT(str, &datetime);
}

