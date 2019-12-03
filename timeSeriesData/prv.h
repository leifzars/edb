/*
 * prv.h
 *
 */

#ifndef TIMESERIESDATA_TSD_PRV_H_
#define TIMESERIESDATA_TSD_PRV_H_

#include "config.h"
#if INCLUDE_TSD

#include "os.h"
#include "env.h"
#include "time.h"

#include "lib/vector.h"

#include "tsd.h"
#include "stagingBufferHeap.h"
#include "cursor.h"
#include "series.h"
#include "header.h"
#include <edb/edb_lib.h>


#define POINT_BLOB_REGION_START		0x10000000
#define POINT_BLOB_REGION_END		0x1FFFFFFF
#define IS_POINT_BLOB_KEY(key)		(key & POINT_BLOB_REGION_START)

#define TSD_SERIES_TBL_ID			0x01
#define TSD_POINT_TBL_ID			0x02

typedef uint32_t TSD_BlobKey_t;

typedef struct {
	timeTicks_t oldestPoint;
	timeTicks_t newestPoint; //dateTimePacked_t
	uint32_t count;
	uint32_t consumedSpace;

} tsd_seriesState_t;


typedef struct {
	TSD_dataTypes_e pointType;
	bool isPointReference;
	timePeriod_t maxAge;
	uint32_t maxStorage;
} tsd_seriesconfig_t;

typedef struct {
	TSD_SeriesId_t id;
	tsd_seriesState_t state;
	tsd_seriesconfig_t config;

} TSD_SeriesPersistedInfo_t;

typedef struct {
	uint8_t version;
	uint16_t seriesCount;
	TSD_SeriesId_t nextSeriesId;
	timePeriod_t maxAge;
} TSD_HistorianHeader_t;

typedef struct {
	EDB_Hndl_t edbHndl;
	edb_tblHndl_t pntTbl;
	EDB_xHndl_t wrtTrns;
	EDB_xHndl_t readTrns;

	edb_tblHndl_t srsTbl;
	TSD_HistorianHeader_t h;

	_OS_MUTEX_TYPE lock;

//	uint16_t seriesCount;
//	TSD_SeriesId_t nextSeriesId;
//	timePeriod_t maxAge;
	vec_t seriesVector;
	uint32_t minPeriodPointCount;
	uint32_t flushing : 1;
} TSD_Historian_t;

typedef struct {

	timeTicks_t periodStart;
	timeTicks_t lastPoint;		//dateTimePacked_t

	uint32_t count;

} TSD_PeriodHeader_t;

typedef PRE_PACKED struct {
	uint32_t offset_mil;

	union {
		uint8_t Byte;
		uint16_t Word;
		uint32_t Int;
		float Float;
		TSD_BlobKey_t blobKey;
	};
}POST_PACKED tsd_point_t;

struct stagedPoint {
	struct stagedPoint* next;

	uint32_t timeOffset_mil;

	union {
		uint8_t Byte;
		uint16_t Word;
		uint32_t Int;
		float Float;
		TSD_BlobKey_t blobKey;
	};
};

typedef struct stagedPoint stagedPoint_t;

typedef struct {
	uint32_t key;
	TSD_PeriodHeader_t fpValueHeader;
	FPM_Object_t fpValue;
	uint16_t stagedOldest;
	uint16_t stagedNewest;
} periodInfo_t;

typedef struct {
	TSD_SeriesId_t id;

	tsd_seriesState_t state;
	tsd_seriesconfig_t config;

	struct {
		stagedPoint_t* oldest;
		stagedPoint_t* newest;
		uint32_t count;
		//timeEpoch_mil_t tsOffset;
	} staged;

	struct {
		uint32_t key;
		pgIdx_t valuePgIdx;
		timeTicks_t periodStart;
		uint32_t count;
	} currenyPeriod;
} seriesInfo_t;


extern TSD_Historian_t tsd;

seriesInfo_t* tsd_getSeries(TSD_SeriesId_t id);
seriesInfo_t* tsd_getSeriesWithLargestStagedBuffer();
TSD_Result_e tsd_loadSeriesHeaders();
void tsd_unLoadSeriesHeaders();
TSD_Result_e tsd_writeSeriesHeader(seriesInfo_t* s);
void tsd_seriesInit();

void tsd_writeXStart();
void tsd_writeXEnd();
void tsd_writeXOpEnd();
void tsd_xEnd();

uint16_t tsd_calcKey_hour(timeTicks_t time);
uint32_t tsd_calcPeriodKey(TSD_SeriesId_t id, timeTicks_t time);
uint32_t tsd_calcPeriodKey_first(TSD_SeriesId_t id);
uint32_t tsd_calcPeriodKey_last(TSD_SeriesId_t id);

void tsd_allocStagingBufferHeap(uint32_t pointCount);
void tsd_freeStagingBufferHeap();
stagedPoint_t* tsd_getStagePoint();
void tsd_returnStagePoint(stagedPoint_t* sp);

TSD_Result_e tsd_truncate(seriesInfo_t* s, timeTicks_t truncateUpToPeriod);

TSD_PeriodHeader_t* TSD_cursorGetPeriodHeader(TSD_cursorHndl_t cursor);
void tsd_cursor_skipToNextPeriod(TSD_cursorHndl_t cursor);

#endif
#endif /* TIMESERIESDATA_TSD_PRV_H_ */
