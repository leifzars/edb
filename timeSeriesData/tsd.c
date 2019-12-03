/*
 * tsd.c
 *
 */

#include "config.h"
#if INCLUDE_TSD

#include "stdlib.h"
#include "string.h"
#include "stdio.h"

#include "lib/assert.h"

#include "prv.h"

TSD_Historian_t tsd;

static TSD_Result_e open(edb_pm_openArgsPtr_t openArgs);

TSD_Result_e stagingBuffer_flush(seriesInfo_t* s, bool closePeriod);

static timeTicks_t calcPeriodStart(timeTicks_t time);

static EDB_result_t period_loadHeader(seriesInfo_t* s, FPM_Object_t* fpObj);
static void period_close(seriesInfo_t* s);

static TSD_Result_e freeSpace();

static TSD_Result_e truncateAll(timeTicks_t truncateUpToPeriod);

static bool getMaxSpan(timeTicks_t* out_oldest, timeTicks_t* out_newest);

static void initNewCurrentPeriod(seriesInfo_t* s, timeTicks_t time);


void TSD_init() {
	TSD_Result_e err;

	_OS_MUTEX_INIT(&tsd.lock, 0);

	tsd_allocStagingBufferHeap(TSD_STAGING_HEAP_SIZE_IN_PT_COUNT);
	tsd_seriesInit();
}

TSD_Result_e TSD_open(const char* file, bool resetAll) {
	TSD_Result_e err;

	if (tsd.edbHndl)
		return TSD_ERROR_IN_USE;

	edb_pm_openArgs_t openArgs = { resetAll,	//resetAll
			false,	//resetOnCorruption
			true,	//isFile
			file,	//fileName
			0,	//firstPageOffset
			0,	//maxPageCount
			true, //useWAL
			};

#if TSD_USE_RAW_PARTITION
//	if (file == NULL) {
//		APP_LIB_RP_regionInfo_t ri;
//		APP_LIB_RP_getRegion(APP_LIB_RP_TSD_DB, &ri);
//
//		openArgs.isFile = false;
//		openArgs.fileName = ri.fileName;
//		openArgs.firstPageOffset = ri.startPage;
//		openArgs.maxPageCount = ri.pagegCount;
//	}
#endif

#if TSD_QA_PURGE_LIFO
	openArgs.maxPageCount = 500; //3000
#endif

	err = open(&openArgs);
	if (err) {
		openArgs.resetAll = true;
		EDB_PRINT_E( "Failed to OPEN %s So Deleting", openArgs.fileName);
		if (file)
			remove(file);
		err = open(&openArgs);
	}

	return err;
}

TSD_Result_e open(edb_pm_openArgsPtr_t openArgs) {
	TSD_Result_e err;

	if ((err = EDB_open(&tsd.edbHndl, openArgs)))
		return err;

#if TSD_ENABLE_COMBINED_PERIOD
	tsd.minPeriodPointCount = TSD_MIN_POINTS_PER_COMBINED_PERIOD;
#endif

	tsd_writeXStart();

	if (tsd.edbHndl->isNew) {
		tsd_initHeader();

		if ((err = EDB_tableAdd(tsd.wrtTrns, TSD_SERIES_TBL_ID, &tsd.pntTbl)))
			goto failed;

		tsd.srsTbl = tsd.pntTbl;

		if ((err = tsd_writeHeader()))
			goto failed;

		EDB_PRINT_I( "TSD DB Opened New");
	} else {

		if ((err = EDB_tableGetOrCreate(tsd.wrtTrns, TSD_SERIES_TBL_ID, &tsd.pntTbl))) {
			goto failed;
		}

		tsd.srsTbl = tsd.pntTbl;
		if ((err = tsd_readHeader())) {
			if (err == EDB_NOT_FOUND) {
				//Probably due to New TSD, is fully initialized.
			}
			goto failed;
		}
		if ((err = tsd_loadSeriesHeaders()))
			goto failed;

		EDB_PRINT_I( "TSD DB Opened");
	}

	//EDB_flush(tsd.edbHndl);

	failed:

	tsd_writeXEnd();

	if (err) {
		if (tsd.edbHndl) {
			EDB_close(tsd.edbHndl);
			tsd.edbHndl = NULL;
		}
		tsd_unLoadSeriesHeaders();
	}

	return err;
}

TSD_Result_e TSD_clear() {
	TSD_Result_e err = EDB_OKAY;
	TSD_close();

#if TSD_USE_RAW_PARTITION
	err = TSD_open(NULL, true);
#endif

	return err;
}

TSD_Result_e TSD_close() {
	//TSD_Result_e err;

	if (tsd.edbHndl == NULL)
		return TSD_DOES_NOT_EXIST;

	TSD_flush();

	tsd_unLoadSeriesHeaders();

	tsd_initHeader();

	EDB_close(tsd.edbHndl);
	tsd.edbHndl = NULL;

	EDB_PRINT_I( "TSD DB Closed");

	return TSD_OKAY;
}

TSD_Result_e TSD_flush() {
	TSD_Result_e err;
	seriesInfo_t* s;

	err = (TSD_Result_e) _OS_MUTEX_LOCK(&tsd.lock);
	_ASSERT_DEBUG(err == 0);

	tsd_writeXStart();

	for (uint32_t i = 0; i < tsd.seriesVector.size; i++) {
		s = VEC_GET_PTR((&tsd.seriesVector), seriesInfo_t*, i);
		err = stagingBuffer_flush(s, true);
	}

	err = tsd_writeHeader();

	tsd_writeXEnd();

	//err = (TSD_Result_e) EDB_flush(tsd.edbHndl);

	_OS_MUTEX_UNLOCK(&tsd.lock);

	return err;
}

EDB_result_t period_loadHeader(seriesInfo_t* s, FPM_Object_t* fpObj) {
	EDB_result_t fErr;
	TSD_PeriodHeader_t ph;

	if (!(fErr = EDB_obj_read(fpObj, (void*) &ph, sizeof(TSD_PeriodHeader_t)))) {
		s->currenyPeriod.count = ph.count;
		_ASSERT_DEBUG(s->currenyPeriod.periodStart == ph.periodStart);
	}

	return fErr;
}

EDB_result_t period_update(seriesInfo_t* s, FPM_Object_t* fpObj) {
	EDB_result_t fErr;
	buf_t data;
	TSD_PeriodHeader_t ph;

	ph.count = s->currenyPeriod.count;
	ph.periodStart = s->currenyPeriod.periodStart;
	ph.lastPoint = s->state.newestPoint;

	data.data = (void*) &ph;
	data.dataLen = sizeof(TSD_PeriodHeader_t);

	FPM_objectScanToStart(fpObj);

	fErr = EDB_obj_write(fpObj, &data);

	return fErr;
}
void period_close(seriesInfo_t* s) {

	s->currenyPeriod.key = 0;
	s->currenyPeriod.valuePgIdx = 0;

}

TSD_Result_e stagingBuffer_flush(seriesInfo_t* s, bool closePeriod) {
	FPM_Object_t fpObj;
	TSD_Result_e err;
	tsd_point_t pointBuffer[TSD_SERIES_STAGING_BUFFER_MAX];
	stagedPoint_t* p;
	stagedPoint_t* pp;
	uint32_t i;
	buf_t data;

	if (tsd.flushing) //needed to deal with iterator
		return TSD_OKAY;
	tsd.flushing = true;

	tsd.flushing = false;

	if (s->staged.count == 0 && !closePeriod)
		return TSD_OKAY;

	tsd_writeXStart();

	if (s->staged.count != 0) {

		if (s->currenyPeriod.valuePgIdx == 0) {
			if ((err = (TSD_Result_e) EDB_openValue(tsd.wrtTrns, tsd.pntTbl, s->currenyPeriod.key, &fpObj)))
				return err;
			s->currenyPeriod.valuePgIdx = FPM_objectPageIndex(&fpObj);
			_ASSERT_DEBUG(s->currenyPeriod.valuePgIdx < tsd.edbHndl->fpm.s.usedPageCount);
			_ASSERT_DEBUG(FPM_objectPageIndex(&fpObj) == s->currenyPeriod.valuePgIdx);

		} else {
			_ASSERT_DEBUG(s->currenyPeriod.valuePgIdx < tsd.edbHndl->fpm.s.usedPageCount);
			if ((err = (TSD_Result_e) EDB_openValue_ByPage(tsd.wrtTrns, &fpObj, s->currenyPeriod.valuePgIdx)))
				return err;
			_ASSERT_DEBUG(FPM_objectPageIndex(&fpObj) == s->currenyPeriod.valuePgIdx);
		}

		if (FPM_objectLen(&fpObj) == 0) {
			TSD_PeriodHeader_t ph;
			memset((void*) &ph, 0, sizeof(TSD_PeriodHeader_t));

			data.data = (void*) &ph;
			data.dataLen = sizeof(TSD_PeriodHeader_t);

			//write zeros to advance the cursor
			if ((err = (TSD_Result_e) EDB_obj_write(&fpObj, &data))) {
				_ASSERT(0);
				return err;
			}
#if TSD_ENABLE_DEBUG_PRINT
			EDB_PRINT_I( "Period new.   s %d k 0x%04x", s->id, s->currenyPeriod.key);
#endif
		} else {
			if (s->currenyPeriod.count == 0) {
				period_loadHeader(s, &fpObj);
			}

			if ((err = EDB_obj_scanToEnd(&fpObj))) {
				_ASSERT(0);
				return (TSD_Result_e) err;
			}
		}

		if (s->state.oldestPoint == 0)
			s->state.oldestPoint = s->currenyPeriod.periodStart
					+ ((uint64_t) s->staged.oldest->timeOffset_mil * TIME_TICKS_IN_MIL_SEC);

		s->state.newestPoint = s->currenyPeriod.periodStart
				+ ((uint64_t) s->staged.newest->timeOffset_mil * TIME_TICKS_IN_MIL_SEC);

		p = s->staged.oldest;
		i = 0;
		do {
			//pointOffset = s->currenyPeriod.periodStart + ((uint64_t) p->timeOffset_mil * TIME_TICKS_IN_MIL_SEC);
			pointBuffer[i].Int = p->Int;
			pointBuffer[i].offset_mil = p->timeOffset_mil;

			pp = p;
			p = p->next;

			tsd_returnStagePoint(pp);
			i++;
		} while (p != NULL);

		s->staged.oldest = NULL;
		s->staged.newest = NULL;
		s->staged.count = 0;

		s->state.count += i;
		s->currenyPeriod.count += i;

		data.data = (void*) pointBuffer;
		data.dataLen = sizeof(tsd_point_t) * i;

		if ((err = (TSD_Result_e) EDB_obj_write(&fpObj, &data))) {
			_ASSERT(0);
			return err;
		}

#if TSD_ENABLE_DEBUG_PRINT
		EDB_PRINT_I( "Flush. s %d k %d fSize %d pSize %d -> %d", s->id, s->currenyPeriod.key, i,
				s->currenyPeriod.count, i + s->currenyPeriod.count);
#endif

		err = period_update(s, &fpObj);
		_ASSERT_DEBUG(err == 0);

		EDB_obj_close(&fpObj);
	}

	if (closePeriod) {
#if TSD_ENABLE_DEBUG_PRINT
		EDB_PRINT_I( "Period close. s %d k 0x%04x pCnt %d", s->id, s->currenyPeriod.key, s->currenyPeriod.count);
#endif
		period_close(s);

		err = (TSD_Result_e) tsd_writeSeriesHeader(s);
		_ASSERT_DEBUG(err == 0);
	}

	tsd_writeXOpEnd();

	return TSD_OKAY;
} //

void tsd_writeXStart() {
	if (edb_pm_availablePages(tsd.edbHndl) < 100) {
		if (tsd.wrtTrns != NULL) {
			EDB_trns_commit(tsd.wrtTrns);
			tsd.wrtTrns = NULL;
		}
		freeSpace();
	}

	if (tsd.wrtTrns == NULL)
		EDB_trns_start(tsd.edbHndl, &tsd.wrtTrns, EDB_X_WRITER);
}
void tsd_writeXEnd() {
	EDB_trns_commit(tsd.wrtTrns);
	tsd.wrtTrns = NULL;
}

void tsd_writeXOpEnd() {
	if (tsd.wrtTrns) {
		if (tsd.wrtTrns->pageRefs.size > 128 || EDB_trns_percentSpaceRemaing(tsd.edbHndl, tsd.wrtTrns) < 10 )
			tsd_writeXEnd();
	}
}

void tsd_xEnd() {
	if (tsd.wrtTrns) {
		tsd_writeXEnd();
	}
}

bool getMaxSpan(timeTicks_t* out_oldest, timeTicks_t* out_newest) {
	seriesInfo_t* s;
	seriesInfo_t* oldestS = NULL;
	seriesInfo_t* newestS = NULL;

	for (uint32_t i = 0; i < tsd.seriesVector.size; i++) {
		s = VEC_GET_PTR((&tsd.seriesVector), seriesInfo_t*, i);

		if ((oldestS == NULL && s->state.count)
				|| (oldestS != NULL && s->state.oldestPoint < oldestS->state.oldestPoint)) {
			oldestS = s;
		}
		if ((newestS == NULL && s->state.count)
				|| (newestS != NULL && s->state.newestPoint > newestS->state.newestPoint)) {
			newestS = s;
		}
	}

	if (oldestS == NULL) {
		return false;
	}

	*out_oldest = oldestS->state.oldestPoint;
	*out_newest = newestS->state.newestPoint;

	return true;
}

TSD_Result_e freeSpace() {
	timeTicks_t oldest;
	timeTicks_t newest;
	uint32_t prunToHr;
	uint32_t hrsToPrun;

	if (!getMaxSpan(&oldest, &newest))
		return TSD_OKAY;

	uint64_t deltaHours = (newest / TIME_TICKS_IN_HR) - (oldest / TIME_TICKS_IN_HR);

	//Prune 1 hour worst case

	if (deltaHours <= 1) {
		//This is an issue
		hrsToPrun = 1;
	} else if (deltaHours < 24) {
		hrsToPrun = 1;
	} else {
		hrsToPrun = (uint32_t) (deltaHours * .1);
	}

	prunToHr = (uint32_t) ((oldest / TIME_TICKS_IN_HR) + hrsToPrun);

//#if TSD_DIAG_TRUNCATE
	EDB_PRINT_I( "Pruning from tail %d periods", hrsToPrun);
//#endif

	truncateAll(prunToHr);

	return TSD_OKAY;
}

TSD_Result_e truncateAll(timeTicks_t truncateUpToPeriod) {
	TSD_Result_e err;
	seriesInfo_t* s;

#if TSD_DIAG_PRINT
	uint32_t initialAvailablePages;
	uint32_t deleteCt;
	uint32_t initialCt;

	deleteCt = 0;
	initialAvailablePages = FPM_availablePages(&tsd.edbHndl->);
#endif

	for (uint32_t i = 0; i < tsd.seriesVector.size; i++) {
		s = VEC_GET_PTR((&tsd.seriesVector), seriesInfo_t*, i);
#if TSD_DIAG_PRINT
		initialCt = s->state.count;
#endif
		err = tsd_truncate(s, truncateUpToPeriod);

#if TSD_DIAG_PRINT
		deleteCt += initialCt - s->state.count;
#endif
	}

#if TSD_DIAG_PRINT
	EDB_PRINT_I( "Total pt dropped %d, recoverd pages %d", deleteCt, FPM_availablePages(&tsd.edbHndl->) - initialAvailablePages);
#endif

	return TSD_OKAY;
}

TSD_Result_e tsd_truncate(seriesInfo_t* s, timeTicks_t truncateUpToPeriod) {
	TSD_cursorHndl_t cursor;
	TSD_Point_t p;
	TSD_cursorStatus_t tsdCResult;
	timeTicks_t sNewOldestPoint;
	TSD_PeriodHeader_t* cph;
	uint32_t sDeleteCt;
	const uint32_t batchSize = 48;
	uint32_t keysToPrun[batchSize];
	uint32_t keysToPrunIdx;

	if (truncateUpToPeriod == 0)
		truncateUpToPeriod = TIME_maxTick;

	if (s->state.count == 0)
		return TSD_OKAY;

	do {
		memset(keysToPrun, 0, sizeof(keysToPrun));
		keysToPrunIdx = 0;

		tsd_writeXStart();

		sDeleteCt = 0;
		sNewOldestPoint = 0;
		cursor = TSD_cursorInitAll(s->id, TSD_ACS);

		while ((tsdCResult = TSD_cursorFindNext(cursor, &p)) == TSD_CURSOR_ACTIVE) {
			extern TSD_PeriodHeader_t* tsd_cursor_getPeriodHeader(TSD_cursorHndl_t cursor);
			cph = tsd_cursor_getPeriodHeader(cursor);

			if ((cph->lastPoint / TIME_TICKS_IN_HR) < truncateUpToPeriod) {

				sDeleteCt += cph->count;
				timeTicks_t tmpPeriodStart = cph->periodStart;
				keysToPrun[keysToPrunIdx++] = tsd_calcPeriodKey(s->id, tmpPeriodStart);

#if TSD_DIAG_TRUNCATE
				EDB_PRINT_I( "Deleting sid: %d hr %d period %d", s->id, tsd_calcKey_hour(tmpPeriodStart),
						tsd_calcPeriodKey(s->id, tmpPeriodStart));
#endif

				tsd_cursor_skipToNextPeriod(cursor);

			} else {
				sNewOldestPoint = p.time;
				break;
			}
			if (keysToPrunIdx == batchSize) {
				sNewOldestPoint = p.time;
				break;
			}
		}

		for (uint32_t j = 0; j < keysToPrunIdx; j++) {
			EDB_delete(tsd.wrtTrns, tsd.pntTbl, keysToPrun[j]);
		}

		tsd_writeXEnd();

	} while (keysToPrunIdx);

	if (sNewOldestPoint) {
#if TSD_DIAG_TRUNCATE
		char from[32];
		char to[32];
		TIME_toString(from, s->state.oldestPoint);
		TIME_toString(to, sNewOldestPoint);
		EDB_PRINT_I( "Series %d Tail Truncated. ct %d from %s to %s", s->id, sDeleteCt, from, to);
#endif
		s->state.oldestPoint = sNewOldestPoint;
		s->state.count -= sDeleteCt;
	} else {
#if TSD_DIAG_TRUNCATE
		char from[32];
		TIME_toString(from, s->state.oldestPoint);
		EDB_PRINT_I( "Series %d Tail Truncated. ct %d from %s to ALL", s->id, sDeleteCt, from);
#endif
		s->state.newestPoint = 0;
		s->state.oldestPoint = 0;
		s->state.count = 0;
	}

	TSD_cursorReturn(cursor);

	return TSD_OKAY;
}

void initNewCurrentPeriod(seriesInfo_t* s, timeTicks_t time) {
	_ASSERT_DEBUG(s->currenyPeriod.key == 0);
	_ASSERT_DEBUG(s->staged.count == 0);

	s->currenyPeriod.periodStart = calcPeriodStart(time);
	s->currenyPeriod.key = tsd_calcPeriodKey(s->id, time);
	s->currenyPeriod.count = 0;

#if TSD_ENABLE_DEBUG_PRINT
	EDB_PRINT_I( "Period Start. s %d key %dpStart @ %lld", s, s->currenyPeriod.key, s->currenyPeriod.periodStart);
#endif

}

TSD_Result_e TSD_writePoint(TSD_SeriesId_t id, timeTicks_t time, void* value) {
	uint64_t periodOffset;
	TSD_Result_e r;
	seriesInfo_t* s;
	stagedPoint_t* sp;
	timeTicks_t previousPointTime;
	//milSecs_t time;

	r = _OS_MUTEX_LOCK(&tsd.lock);
	_ASSERT(r == 0);

	s = tsd_getSeries(id);

	//insure time is in whole milliseconds
	time = time - (time % TIME_TICKS_IN_MIL_SEC);

	if (s == NULL) {
		r = TSD_DOES_NOT_EXIST;
		goto err;
	}

	if (s->staged.count) {
		previousPointTime = s->currenyPeriod.periodStart
				+ (uint64_t) s->staged.newest->timeOffset_mil * TIME_TICKS_IN_MIL_SEC;
	} else if (s->state.count) {
		previousPointTime = s->state.newestPoint;
	} else {
		previousPointTime = 0;
	}

	if (previousPointTime) {
		if (previousPointTime >= time) {
			_ASSERT_DEBUG(0);
			if (previousPointTime == time)
				r = TSD_WRITE_FAILED_TS_EXISTS;
			else
				r = TSD_WRITE_FAILED_TS_PAST;
			goto err;
		}
	}

	if (s->currenyPeriod.key == 0) {
		initNewCurrentPeriod(s, time);
	}
	periodOffset = time - s->currenyPeriod.periodStart;

	bool doClosePeriod;
#if TSD_ENABLE_COMBINED_PERIOD
	if (tsd.minPeriodPointCount) {
		doClosePeriod = (s->staged.count + s->currenyPeriod.count) >= tsd.minPeriodPointCount;
		if (doClosePeriod) {
			bool startsNewPeriod = previousPointTime != 0
					&& calcPeriodStart(time) != calcPeriodStart(previousPointTime);
			doClosePeriod &= startsNewPeriod;
		}
	} else
#endif
	{
		doClosePeriod = periodOffset >= TIME_SEC_TO_TICKS(TSD_PERIOD_SPAN_SEC);
	}

	if (doClosePeriod) {
		if ((r = stagingBuffer_flush(s, true))) {
			_ASSERT_DEBUG(0);
			goto err;
		}
		initNewCurrentPeriod(s, time);

		periodOffset = time - s->currenyPeriod.periodStart;
	}

	sp = tsd_getStagePoint();
	if (sp == NULL) {
		stagingBuffer_flush(tsd_getSeriesWithLargestStagedBuffer(), false);
		sp = tsd_getStagePoint();
	}
	_ASSERT(sp != NULL);

	sp->Int = *(uint32_t*) value;
	sp->timeOffset_mil = (uint32_t) (periodOffset / TIME_TICKS_IN_MIL_SEC);
	sp->next = NULL;

	if (s->staged.newest)
		s->staged.newest->next = sp;
	s->staged.newest = sp;
	s->staged.count++;

	if (s->staged.oldest == NULL) {
		s->staged.oldest = sp;
	}

	if (s->staged.count >= TSD_SERIES_STAGING_BUFFER_MAX) {
		stagingBuffer_flush(s, false);
	}

	err:

	_OS_MUTEX_UNLOCK(&tsd.lock);

	return r;
}

vec_t* TSD_seriesGetIds(uint32_t rangeFrom, uint32_t rangeTo) {
	seriesInfo_t* s;

	_OS_MUTEX_LOCK(&tsd.lock);

	vec_t* vec = (vec_t*) _OS_MAlloc(sizeof(vec_t));
	vec_init(vec, sizeof(TSD_SeriesId_t), 8);
	for (uint32_t i = 0; i < tsd.seriesVector.size; i++) {
		s = VEC_GET_PTR((&tsd.seriesVector), seriesInfo_t*, i);
		if ((rangeFrom == 0 && rangeTo == 0) || (s->id >= rangeFrom && s->id <= rangeTo)) {
			VEC_ADD(vec, &s->id);
		}
	}

	_OS_MUTEX_UNLOCK(&tsd.lock);

	return vec;
}

TSD_Result_e TSD_seriesGetInfo(TSD_SeriesId_t id, TSD_seriesInfo_t* out_si) {
	seriesInfo_t* s;

	_OS_MUTEX_LOCK(&tsd.lock);

	s = tsd_getSeries(id);

	if (s == NULL) {
		_OS_MUTEX_UNLOCK(&tsd.lock);
		return TSD_DOES_NOT_EXIST;
	}

	memcpy(&out_si->config, &s->config, sizeof(TSD_seriesConfig_t));
	memcpy(&out_si->state, &s->state, sizeof(TSD_seriesState_t));

	out_si->state.count += s->staged.count;

	if (s->staged.count) {
		out_si->state.newestPoint = s->currenyPeriod.periodStart
				+ ((uint64_t) s->staged.newest->timeOffset_mil * TIME_TICKS_IN_MIL_SEC);
	} else {
		out_si->state.newestPoint = s->state.newestPoint;
	}

	if (s->state.count)
		out_si->state.oldestPoint = s->state.oldestPoint;
	else
		out_si->state.oldestPoint = s->currenyPeriod.periodStart
				+ ((uint64_t) s->staged.oldest->timeOffset_mil * TIME_TICKS_IN_MIL_SEC);

	_OS_MUTEX_UNLOCK(&tsd.lock);
	return TSD_OKAY;
}

timeTicks_t calcPeriodStart(timeTicks_t time) {
	return time - (time % TIME_TICKS_IN_HR);	//SEC_TO_MILISEC(TIME_SECONDS_PER_HOUR));
}
/*
 * #if TSD_COMBINE_SERIES_POINT_TBL
 *  XXXX      YYYY
 *  ^ 		  ^
 *  SeriesID  (Hour % 0xFFFF)
 * #else
 * 	Does not work
 * #endif
 */
uint32_t tsd_calcPeriodKey(TSD_SeriesId_t id, timeTicks_t time) {

	uint32_t timeKey = tsd_calcKey_hour(time);
	uint32_t key = (uint32_t) id << 16;
	key |= timeKey;
	return key;
}

uint32_t tsd_calcPeriodKey_first(TSD_SeriesId_t id) {
	uint32_t timeKey = 0;
	uint32_t key = (uint32_t) id << 16;
	key |= timeKey;
	return key;
}

uint32_t tsd_calcPeriodKey_last(TSD_SeriesId_t id) {
	uint32_t timeKey = 0xFFFF;
	uint32_t key = (uint32_t) id << 16;
	key |= timeKey;
	return key;
}
uint16_t tsd_calcKey_hour(timeTicks_t time) {
	uint32_t hours = time / TIME_TICKS_IN_HR;	//SEC_TO_MILISEC(TIME_SECONDS_PER_HOUR);
	return (uint16_t) (hours % 0xFFFF);
}
#endif

