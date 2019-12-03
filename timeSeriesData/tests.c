/*
 * tsd_tests.c
 *
 */

#include "config.h"
#if INCLUDE_TSD

#include "stdlib.h"
#include "time.h"
#include "time/timeHelp.h"
#include <timeSeriesData/prv.h>

typedef struct {
	TSD_SeriesId_t seriesId;
	timeTicks_t lastTime;
	uint32_t valueToInc;
	uint32_t interval_ms;
} seriesTestState_t;

static void testCombinedPeriods_at1800sec(uint32_t days);
static void testWriteIterateAll(uint32_t days);
static uint32_t iterateAll(TSD_SeriesId_t seriesId, TSD_order_t order);
static uint32_t writePoints(seriesTestState_t* series, uint32_t numOfSeries, timeTicks_t startTime, timeTicks_t endTime);

#define NUMBER_OF_SERIES	10

static const char fileName[] = "tsd.db";


void TSD_test() {
	TSD_Result_e err;

#if TSD_QA_ENABLE_RAW
	err = TSD_open(NULL, true);
#else
	remove(fileName);
	err = TSD_open(fileName, true);
#endif

	_ASSERT_DEBUG(!err);

	testCombinedPeriods_at1800sec(30);
	testWriteIterateAll(60);

	TSD_close();
}
void exampleOpen()
{
	//#if TSD_USE_RAW_PARTITION
	//	err = TSD_open(NULL, false);
	//#else
	//	err = TSD_open(fileName, false);
	//	if (err == (TSD_Result_e) EDB_PM_FAILED_BAD_STATE) {
	//		remove(fileName);
	//		err = TSD_open(fileName, false);
	//	}
	//#endif
	//	_ASSERT_DEBUG(!err);
}
/* 11/9/2017
 * 10 Series 48Hr
 * 760000 writes in 19:31.1 - 24:10.7 = 4:39.8 = 279.8 sec
 * ~2,724 pts per sec
 * Iterate All (375,680) in   12.6 sec
 * ~29,815 reads per sec
 */
void testWriteIterateAll(uint32_t days) {
	timeTicks_t yesterday = TIME_now();
	timeTicks_t endTime;
	uint32_t ptsWritten;

	srand(time(0));

	EDB_PRINT_QA("Testing - TSD Series: %d, Period: Simulated %d days",NUMBER_OF_SERIES, days);

	yesterday -= TIME_TICKS_IN_DAY; //SEC_TO_MILISEC(DAYS_TO_SEC(1));

	seriesTestState_t sArray[NUMBER_OF_SERIES];

	for (int i = 0; i < NUMBER_OF_SERIES; i++) {
		sArray[i].seriesId = TSD_seriesCreate(tsd_uint32);
		_ASSERT_DEBUG(sArray[i].seriesId > 0);

		sArray[i].lastTime = yesterday;
		sArray[i].valueToInc = 0;

		uint32_t rnd;
		rnd = rand();

		sArray[i].interval_ms = (rnd % ( TSD_QA_RATE_RAGNE_FROM_MS - TSD_QA_RATE_RAGNE_TO_MS + 1)) + TSD_QA_RATE_RAGNE_TO_MS;

		EDB_PRINT_QA("\t\tSeries %d @ interval %d ms", sArray[i].seriesId , sArray[i].interval_ms);
	}

	endTime = TIME_TICKS_IN_HR * 24 * days;
	endTime += yesterday;

	ptsWritten = writePoints(sArray, NUMBER_OF_SERIES, yesterday, endTime);

	TSD_flush();

	uint32_t pointsFound = 0;
	uint32_t sPointFound;
	for (int i = 0; i < NUMBER_OF_SERIES; i++) {
		sPointFound = iterateAll(sArray[i].seriesId, TSD_ACS);
		pointsFound += sPointFound;
		_ASSERT(sPointFound == sArray[i].valueToInc);
	}

	_ASSERT(pointsFound == ptsWritten);
	EDB_PRINT_QA( "Points found ACS %d", pointsFound);


	uint32_t sPointFoundDesc;
	sPointFoundDesc = 0;
	for (int i = 0; i < NUMBER_OF_SERIES; i++) {
		sPointFound = iterateAll(sArray[i].seriesId, TSD_DESC);
		sPointFoundDesc += sPointFound;
		_ASSERT(sPointFound == sArray[i].valueToInc);
	}
	_ASSERT(sPointFoundDesc == ptsWritten);
	_ASSERT(pointsFound == sPointFoundDesc);
	EDB_PRINT_QA( "Points found DESC %d", pointsFound);

}

uint32_t writePoints(seriesTestState_t* series, uint32_t numOfSeries, timeTicks_t startTime, timeTicks_t endTime) {

	seriesTestState_t* s;
	timeTicks_t now = startTime;
	clock_t sw;

	seriesTestState_t* nextS;
	timeTicks_t nextSWriteTime;
	uint32_t pointsWritten = 0;

	sw = clock();

	do {
		nextS = NULL;

		nextSWriteTime = TIME_maxTick;
		for (uint32_t i = 0; i < numOfSeries; i++) {
			s = &series[i];
			if ((s->lastTime + (timeTicks_t) s->interval_ms) < nextSWriteTime) {
				nextSWriteTime = s->lastTime + (timeTicks_t) s->interval_ms;
				nextS = s;
			}
		}

		if (nextS) {
			now = nextS->lastTime + (nextS->interval_ms * TIME_TICKS_IN_MIL_SEC);
			nextS->valueToInc++;
			TSD_writePoint(nextS->seriesId, now, (void*) &nextS->valueToInc);
			nextS->lastTime = now;
			pointsWritten++;
		}
		if ((pointsWritten * TSD_DIAG_SYSTEM_SPEED_ADJ) % 1000 == 0) {
			_EDB_BUSY();
		}

		if (pointsWritten % (5000 * TSD_DIAG_SYSTEM_SPEED_ADJ)== 0) {
			EDB_PRINT_QA( "\twriting points ... %d", pointsWritten);
		}

		if (pointsWritten % (20000 * TSD_DIAG_SYSTEM_SPEED_ADJ) == 0) {
			sw = clock() - sw;
			EDB_PRINT_QA( "Write performance. %f pts/sec", ((20000.0 * TSD_DIAG_SYSTEM_SPEED_ADJ) /sw) * 1000.0);
			EDB_PRINT_QA( "FPM Stats: Space %d/%d pgs", edb_pm_usedPages(tsd.edbHndl), edb_pm_getMaxPageCount(tsd.edbHndl) );

			sw = clock();
		}

		//nextSWriteTime = now;
	} while (now < endTime);
	return pointsWritten;
}

uint32_t iterateAll(TSD_SeriesId_t seriesId, TSD_order_t order) {
	TSD_cursorHndl_t cursor;
	TSD_Point_t p;
	TSD_cursorStatus_t tsdCResult;
	clock_t sw;
	uint32_t pointCount = 0;
int32_t lpv = -1;


	sw = clock();
	cursor = TSD_cursorInitAll(seriesId, order);

	while ((tsdCResult = TSD_cursorFindNext(cursor, &p)) == TSD_CURSOR_ACTIVE) {
		pointCount++;
		if(order == TSD_ACS)
			_ASSERT(pointCount == p.Int);

		if(lpv>=0){
			if(order == TSD_ACS)
				_ASSERT(lpv + 1 == (int32_t)p.Int);
			else
				_ASSERT(lpv - 1 == (int32_t)p.Int);
		}
		lpv = (int32_t)p.Int;
	};
	_ASSERT(tsdCResult == TSD_CURSOR_AT_END);

	TSD_cursorReturn(cursor);
	sw = clock() - sw;


	EDB_PRINT_I( "Iterate All. s %d, ct %d time: %" PRIu64 " mil", seriesId, pointCount, sw);

	_EDB_BUSY();
	return pointCount;
}

uint32_t iterateFrom(TSD_SeriesId_t seriesId, timeTicks_t from, uint32_t valueStart) {
	TSD_cursorHndl_t cursor;
	TSD_Point_t p;
	TSD_cursorStatus_t tsdCResult;
	clock_t sw;
	uint32_t pointCount = 0;

	sw = clock();
	cursor = TSD_cursorInit_range(seriesId, from, 0, TSD_ACS);

	while ((tsdCResult = TSD_cursorFindNext(cursor, &p)) == TSD_CURSOR_ACTIVE) {
		pointCount++;
		_ASSERT((pointCount + valueStart) == p.Int);
	};

	_ASSERT(tsdCResult == TSD_CURSOR_AT_END);

	TSD_cursorReturn(cursor);
	sw = clock() - sw;

	EDB_PRINT_I( "Iterate From. s %d, ct %d time: %" PRIu64 " mil", seriesId, pointCount, sw);

	_EDB_BUSY();
	return pointCount;
}

void testCombinedPeriods_at1800sec(uint32_t days) {
	timeTicks_t start = TIME_now();
	timeTicks_t now = start;
	TSD_SeriesId_t sId;
	clock_t sw;

	sId = TSD_seriesCreate(tsd_uint32);

	sw = clock();
	for (int i = 0; i < 24 * days; i++) {
		uint32_t x = i + 1;
		TSD_writePoint(sId, now, (void*) &x);
		now += TIME_TICKS_IN_HR / 2;
	}
	sw = clock() - sw;
	EDB_PRINT_I( "Write performance. %f pts/sec", (double)(24 * days / ((sw * 1000)+1)));

	iterateAll(sId, TSD_ACS);

	for (int i = 0; i < 24 * days; i++) {
		iterateFrom(sId, start + (TIME_TICKS_IN_HR / 2) * i, i);
	}

}

#endif
