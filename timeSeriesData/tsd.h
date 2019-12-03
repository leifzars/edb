/*
 * tsd.h

 *
 *  TimeSeriesData lib
 *  Quickly and efficiently store and iterate through time series data with 1ms resolution
 *  and 4 byte values. Writes are buffered and grouped
 *  Scanning a series is fast
 *  Writing is buffered for speed
 *  Only single instance/file access supported.
 *
 *  Supported Operations - append & truncate tail
 *  	Not supported - delete, update & insert
 *
 *  Value - 4 Byte value
 *  Point - Value & Time
 *  	Resolution - 1 millisecond
 *  Series - Ordered list of points, or values over time
 *  	Series is identified by TSD_SeriesId_t
 *  	Max Series - 0xFFFF
 *  Period - Storage unit containing a range of points for a series.
 *  	Periods belong to series, each series has its own for a giving time range.
 *		A period is represented in the EDB as 1 object
 *
 *  Writes
 *		Writes must be temporally increasing per series, or will be rejected.
 *		Writes are buffered, and are flushed when the buffer is full, or the
 *		series has TSD_SERIES_STAGING_BUFFER_MAX points in the buffer.
 *		Recommended that TSD_flush be called every X seconds for best results
 *
 *	Reads
 *		Cursor based. Time range or all. Ordered ascending or descending
 *
 *
 *	Series Index Space - Limited to 0xFFFF hours about 7 years
 *  Series will eat its tail when it runs out of index space
 *
 *
 *   *  Time - based on .NET Tick Time, see /time/timeHelp.h
 *
 */

#ifndef TIMESERIESDATA_TSD_H_
#define TIMESERIESDATA_TSD_H_

#include "config.h"
#if INCLUDE_TSD

#include "env.h"
#include "lib/vector.h"

#define TSD_SERIES_ID_UNKNOWN		0

typedef enum {
	TSD_OKAY = 0,
	TSD_ERROR_IN_USE,
	TSD_DOES_NOT_EXIST,
	TSD_FAILED,
	TSD_WRITE_FAILED_TS_EXISTS,
	TSD_WRITE_FAILED_TS_PAST,
	TSD_ERROR_EDB,
	TSD_ERROR_FPM,
	TSD_TYPE_NOT_SUPPORTED,
	TSD_SERIES_CORRUPTED,
	TSD_ERR_ARGUMENT_INVALID,
	TSD_ERR_DESERILAZATION_FAILED,
	TSD_ERR_ARG_VALUE_TYPE_MISMATCH,
} TSD_Result_e;

typedef enum{
	tsd_unknown,
	tsd_int32,
	tsd_uint32,
	tsd_float
}TSD_dataTypes_e;

typedef uint16_t TSD_SeriesId_t;

typedef PRE_PACKED struct {
	TSD_SeriesId_t Id;
	TSD_dataTypes_e type;
	uint32_t count;
	uint32_t consumedSpace;
	timeTicks_t oldest;
	timeTicks_t newest;
}POST_PACKED tsd_seriesInfo_t;

typedef enum
	FORCE_ENUM_SIZE {
		TSD_Q_RANGE, TSD_Q_RANGE_REVERSED, TSD_LESS_THEN_TS_X, TSD_GREATER_THEN_TX_X,
} TSD_queryType_e;

typedef enum {
	TSD_ACS, //ascending
	TSD_DESC //Descending
} TSD_order_t;

typedef struct {
	timeTicks_t oldestPoint;
	timeTicks_t newestPoint;
	uint32_t count;
	uint32_t consumedSpace;

} TSD_seriesState_t;

typedef struct {
	TSD_dataTypes_e pointType;
	bool isPointReference;
	timePeriod_t maxAge;
	uint32_t maxStorage;
} TSD_seriesConfig_t;

typedef struct {
	TSD_seriesConfig_t config;
	TSD_seriesState_t state;
} TSD_seriesInfo_t;

typedef struct {
	TSD_SeriesId_t seriesId;
	timeTicks_t time;
	union {
		uint8_t Byte;
		uint16_t Word;
		uint32_t Int;
		float Float;
		//TSD_BlobKey_t blobKey;
	};
} TSD_Point_t;

/*
 * Call Once
 */
void TSD_init();

void TSD_test();

TSD_Result_e TSD_open(const char* file, bool resetAll);
TSD_Result_e TSD_close();
TSD_Result_e TSD_flush();
TSD_Result_e TSD_clear();

TSD_SeriesId_t TSD_seriesCreate(TSD_dataTypes_e pointType);
TSD_Result_e TSD_seriesDrop(TSD_SeriesId_t id);
TSD_Result_e TSD_seriesGetInfo(TSD_SeriesId_t id, TSD_seriesInfo_t* out_si);
vec_t* TSD_seriesGetIds(uint32_t rangeFrom, uint32_t rangeTo);

TSD_Result_e TSD_writePoint(TSD_SeriesId_t id, timeTicks_t timeTicks, void* value);


#endif
#endif /* TIMESERIESDATA_TSD_H_ */
