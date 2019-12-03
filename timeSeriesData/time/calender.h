/*
 * clender.h
 *
 */

#ifndef TSD_TIME_CALENDER_H
#define TSD_TIME_CALENDER_H


#define MONTH_JANUARY	1
#define MONTH_FEBRUARY	2
#define MONTH_MARCH		3
#define MONTH_DECEMBER	12


#define IS_LEAP_YEAR(Y)     ( ((Y)>0) && !((Y)%4) && ( ((Y)%100) || !((Y)%400) ) )

#ifdef DAYS_IN_YEAR
#undef DAYS_IN_YEAR
#endif

#define	DAYS_IN_YEAR			365
#define DAYS_IN_LEAP_YEAR		366

#endif /* SOURCES_LIB_TIME_CALENDER_H_ */
