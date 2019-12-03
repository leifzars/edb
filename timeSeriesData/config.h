/*
 * config.h
 *
 */

#ifndef TIMESERIESDATA_CONFIG_H_
#define TIMESERIESDATA_CONFIG_H_

#include "env.h"

#include "time/timeHelp.h"


#ifndef INCLUDE_TSD
#define INCLUDE_TSD							1
#endif

//******************** Parameters **************************

/*
 * TSD_VERSION
 * File version
 */
#define TSD_VERSION									1

/*
 *TSD_DEFAULT_MAX_AGE_TICKS
 *Auto pruning default file parameter.
 *Not used.
 */
#define TSD_DEFAULT_MAX_AGE_TICKS					TIME_TICKS_IN_HR * 2

/*
 * TSD_USE_RAW_PARTITION
 * in development
 */
#define TSD_USE_RAW_PARTITION						0

/*
 * TSD_STAGING_HEAP_SIZE_IN_PT_COUNT
 * Point buffer size
 */
#define TSD_STAGING_HEAP_SIZE_IN_PT_COUNT			512

/*
 * TSD_SERIES_STAGING_BUFFER_MAX
 * When to flush a series from the staging buffer to the EDB.
 * Optimally flush the staging buffer when the series is
 *  large enough to fill 1 page
 *
 *  	40 bytes for page header / 36 for overflow page headers
 *  	20 bytes for tsdPeriod header
 *  	8 bytes per point
 *  	56 points
 */
#define TSD_SERIES_STAGING_BUFFER_MAX				56


/*
 * TSD_PERIOD_SPAN_SEC
 * Each period is contained in one EDB value
 */
#define TSD_PERIOD_SPAN_SEC							(TIME_SECONDS_PER_HOUR * 1)

/*
 * TSD_ENABLE_COMBINED_PERIOD
 * When the data is sparse (few points per period) this feature
 * will combine multiple periods into one EDB value.
 * This increase throughput primarily on reads.
 */
#define TSD_ENABLE_COMBINED_PERIOD					1

/*
 * TSD_MIN_POINTS_PER_COMBINED_PERIOD
 * How many points to combine into one period when the period data is sparce
 */
#if TSD_ENABLE_COMBINED_PERIOD
#define TSD_MIN_POINTS_PER_COMBINED_PERIOD			(TSD_SERIES_STAGING_BUFFER_MAX * 10)
#endif


/*
 * TSD_SERIES_PGIDX_START & TSD_SERIES_PGIDX_END
 * Max number of periods per series
 * 0XFFFF - over 7 years of 1hr periods.
 */
#define TSD_SERIES_PGIDX_START					0x1
#define TSD_SERIES_PGIDX_END					0xFFFF


#define TSD_SERIES_ID_RESERVED_START 	1
#define TSD_SERIES_ID_RESERVED_END  	1024

//******************** Testing **************************
#define TSD_QA_ENABLE								1
#define TSD_QA_ENABLE_RAW							0//TSD_QA_ENABLE
#define TSD_QA_PURGE_LIFO							0
#define TSD_ENABLE_DEBUG_PRINT						0

//#define TSD_QA_RATE_LOW
//#define TSD_QA_RATE_MED
#define TSD_QA_RATE_HIGH
//#define TSD_QA_RATE_TURBO

#ifdef TSD_QA_RATE_LOW
#define TSD_QA_RATE_RAGNE_FROM_MS					(SEC_TO_MILISEC(TIME_SECONDS_PER_HOUR * 2))
#define TSD_QA_RATE_RAGNE_TO_MS						(SEC_TO_MILISEC(TIME_SECONDS_PER_HOUR / 2))
#endif

#ifdef TSD_QA_RATE_MED
#define TSD_QA_RATE_RAGNE_FROM_MS					(SEC_TO_MILISEC(TIME_SECONDS_PER_HOUR + 5))
#define TSD_QA_RATE_RAGNE_TO_MS						(SEC_TO_MILISEC(TIME_SECONDS_PER_HOUR / 8))
#endif

#ifdef TSD_QA_RATE_HIGH
#define TSD_QA_RATE_RAGNE_FROM_MS					(SEC_TO_MILISEC(TIME_SECONDS_PER_MINUTE))
#define TSD_QA_RATE_RAGNE_TO_MS						(SEC_TO_MILISEC(1))
#endif

#ifdef TSD_QA_RATE_TURBO
#define TSD_QA_RATE_RAGNE_FROM_MS					(SEC_TO_MILISEC(5))
#define TSD_QA_RATE_RAGNE_TO_MS						(10)
#endif

#define TSD_DIAG_SYSTEM_SPEED_ADJ					10

#define TSD_DIAG_PRINT								0
#define TSD_DIAG_TRUNCATE							1


#define TSD_FILE_NAME							"a:tsd.db"

#endif /* TIMESERIESDATA_CONFIG_H_ */
