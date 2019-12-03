/*
 * cursor.c
 *
 */

#include "config.h"
#if INCLUDE_TSD

#include "stdlib.h"
#include "string.h"
#include "stdio.h"

#include "env.h"
#include "os.h"
#include "time/timeHelp.h"

#include <timeSeriesData/prv.h>
#include "lib/assert.h"
#include "lib/vector.h"

typedef struct {
	TSD_cursorStatus_t status;
#if EDB_ENABLE_X
	edb_tblHndl_t tblHndl;
	EDB_xHndl_t xHndl;
#endif
	BPT_cursor_t bptCursor;

	seriesInfo_t* s;
	timeTicks_t from;
	timeTicks_t to;

	timeTicks_t constrainedFrom;
	timeTicks_t constrainedTo;

	TSD_order_t order;
	bool firstRange;

	FPM_Object_t fpObj;
	TSD_PeriodHeader_t periodHeader;

	tsd_point_t partialPoint;
	uint32_t partialPointFillIndex;
} cursor_t, *cursorHndl_t;

static TSD_cursorStatus_t cursorScanValue(TSD_cursorHndl_t cursor, TSD_Point_t* out_point);
static bool initEDBSearchRange(cursorHndl_t c);
//typedef enum {
//	TSD_CURSOR_UNINITIALIZED,
//	TSD_CURSOR_INITIALIZED,
//	TSD_CURSOR_ACTIVE,
//	TSD_CURSOR_AT_END,
//} TSD_cursorStatus_t;

TSD_cursorHndl_t TSD_cursorInitAll(TSD_SeriesId_t seriesId, TSD_order_t order) {
	return TSD_cursorInit_range(seriesId, TIME_minTick, TIME_maxTick, order);
}

TSD_cursorHndl_t TSD_cursorInit_range(TSD_SeriesId_t seriesId, timeTicks_t from, timeTicks_t to, TSD_order_t order) {
	cursorHndl_t c;
	BPT_Result_e err;
	seriesInfo_t* s = tsd_getSeries(seriesId);

	if (s == NULL)
		return NULL;

	{
		extern TSD_Result_e stagingBuffer_flush(seriesInfo_t* s, bool closePeriod);
		stagingBuffer_flush(s, false); //hack for now, would be better if not needed
		tsd_xEnd();
	}

	c = _OS_MAlloc(sizeof(cursor_t));
	if (c == NULL)
		return NULL;

	memset(c, 0, sizeof(cursor_t));



	if (to == 0)
		to = TIME_maxTick;

	c->s = s;
	c->from = from;
	c->to = to;
	c->constrainedFrom = from - from % TIME_TICKS_IN_MIL_SEC;
	c->constrainedTo = to - to % TIME_TICKS_IN_MIL_SEC;
	c->firstRange = true;
	c->order = order;

	if (s->state.oldestPoint > from)
		c->constrainedFrom = s->state.oldestPoint;

	if (s->state.newestPoint < to)
		c->constrainedTo = s->state.newestPoint;

	err = (TSD_Result_e) _OS_MUTEX_LOCK(&tsd.lock);
	_ASSERT_DEBUG(err == 0);

	EDB_trns_start(tsd.edbHndl, &c->xHndl, EDB_X_READ_ONLY);

	initEDBSearchRange(c);
//
//	uint32_t lowerKey = tsd_calcPeriodKey(seriesId, c->constrainedFrom);
//	uint32_t higherKey = tsd_calcPeriodKey(seriesId, c->constrainedTo);
//
//	if (higherKey < lowerKey) {
//		higherKey = tsd_calcPeriodKey_last(seriesId);
//	}

//	bErr = BPT_initSearch_Range(&tsd.pntTbl.tbl->rowIndex->bptContext, &c->bptCursor, lowerKey, higherKey, (BPT_order_t)order);

//	if (bErr != BPT_OKAY) {
//		c->status = TSD_CURSOR_ERROR;
//	} else {
//
//		c->status = TSD_CURSOR_INITIALIZED;
//	}

	return (TSD_cursorHndl_t) c;
}

bool initEDBSearchRange(cursorHndl_t c) {
	EDB_result_t kErr;

	uint32_t lowerKey = tsd_calcPeriodKey(c->s->id, c->constrainedFrom);
	uint32_t higherKey = tsd_calcPeriodKey(c->s->id, c->constrainedTo);

	if (c->firstRange) {
		if (c->order == TSD_ACS) {
			if (higherKey < lowerKey) {
				higherKey = tsd_calcPeriodKey_last(c->s->id);
			}
		} else {
			if (lowerKey > higherKey) {
				lowerKey = tsd_calcPeriodKey_first(c->s->id);
			}
		}
	} else {
		if (c->order == TSD_ACS) {
			if (higherKey < lowerKey) {
				lowerKey = tsd_calcPeriodKey_first(c->s->id);
			} else {
				return false;
			}
		} else {
			if (lowerKey > higherKey) {
				higherKey = tsd_calcPeriodKey_last(c->s->id);
			} else {
				return false;
			}
		}

		//if (higherKey >= lowerKey)
		//	return false;
		//else
		//	lowerKey = tsd_calcPeriodKey_first(c->s->id);
	}


	//kErr = BPT_initSearch_Range(&tsd.pntTbl.tbl->rowIndex->bptContext, &c->bptCursor, lowerKey, higherKey,
	//		(BPT_order_t) c->order);

	kErr = EDB_idx_initSearch_Range(c->xHndl, tsd.pntTbl->rowIndex, &c->bptCursor, lowerKey, higherKey,
			(BPT_order_t) c->order);
	if (kErr == EDB_ERROR) {
		c->status = TSD_CURSOR_ERROR;
	} else {
		c->status = TSD_CURSOR_INITIALIZED;

#if TSD_ENABLE_COMBINED_PERIOD
		if (c->order == TSD_ACS) {
			if (tsd.minPeriodPointCount
					&& (c->bptCursor.status == BPT_CS_CURSOR_INITIALIZED || c->bptCursor.status == BPT_CS_END_OF_RESULTS)) { // c->bptCursor.status == BPT_CS_END_OF_RESULTS)){
				if (c->bptCursor.bptCursor.key != lowerKey) {
					kvp_key_t oldestPointKey = tsd_calcPeriodKey(c->s->id, c->s->state.oldestPoint);
					kErr = EDB_idx_prevPeriod(c->xHndl, &tsd.pntTbl->rowIndex->bptContext, &c->bptCursor, oldestPointKey,
							higherKey);
				}
			}
		}
#endif
	}

	return true;
}


void TSD_cursorReturn(TSD_cursorHndl_t cursor) {
	cursorHndl_t c = (cursorHndl_t) cursor;

	if (c->fpObj.oph) {
		EDB_obj_close(&c->fpObj);
	}

	EDB_trns_commit(c->xHndl);

	_OS_Free(c);

	_OS_MUTEX_UNLOCK(&tsd.lock);
}

TSD_PeriodHeader_t* tsd_cursor_getPeriodHeader(TSD_cursorHndl_t cursor) {
	cursorHndl_t c = (cursorHndl_t) cursor;

	if (c->fpObj.oph) {
		return &c->periodHeader;
	}

	return NULL;
}

void tsd_cursor_skipToNextPeriod(TSD_cursorHndl_t cursor) {
	cursorHndl_t c = (cursorHndl_t) cursor;
	if (c->fpObj.oph) {
		EDB_obj_close(&c->fpObj);
	}
}

void pointFromStorage(uint64_t periodStart, tsd_point_t* periodPacked, TSD_Point_t* unPacked) {
	//unPacked->seriesId = periodPacked->seriesId;
	unPacked->Int = periodPacked->Int;
	unPacked->time = periodStart;
	unPacked->time += (timeTicks_t) periodPacked->offset_mil * TIME_TICKS_IN_MIL_SEC;
}

/*
 * A Smart man would use a binarary search to speed this up
 *  */
TSD_cursorStatus_t cursorScanValue(TSD_cursorHndl_t cursor, TSD_Point_t* out_point) {
	cursorHndl_t c = (cursorHndl_t) cursor;
	EDB_result_t fErr;
	tsd_point_t p;

	if (c->fpObj.cursor.offset == 0) {
		if ((fErr = EDB_obj_read(&c->fpObj, (void*) &c->periodHeader, sizeof(TSD_PeriodHeader_t)))) {
			if (fErr == EDB_PM_EXCEEDS_EOF_VALUE)
				return TSD_CURSOR_AT_END; //TSD_CURSOR_ERROR
			else
				return TSD_CURSOR_ERROR_FPM;
		}
		if (c->order == TSD_DESC) {
			EDB_obj_scanToEnd(&c->fpObj);
		}
	}

	if (c->order == TSD_ACS) {
		fErr = EDB_obj_read(&c->fpObj, (void*) &p, sizeof(tsd_point_t));
	} else {
		if (c->fpObj.cursor.offset == sizeof(TSD_PeriodHeader_t))
			return TSD_CURSOR_AT_END;
		fErr = EDB_obj_readR(&c->fpObj, (void*) &p, sizeof(tsd_point_t));
	}

	if (fErr) {
		if (fErr == EDB_PM_EXCEEDS_EOF_VALUE)
			return TSD_CURSOR_AT_END;
		else
			return TSD_CURSOR_ERROR_FPM;
	}

	pointFromStorage(c->periodHeader.periodStart, &p, out_point);

	return TSD_CURSOR_ACTIVE;
}

bool inRange(TSD_cursorHndl_t cursor, TSD_Point_t* point) {
	cursorHndl_t c = (cursorHndl_t) cursor;
	return point->time >= c->constrainedFrom;
}

TSD_cursorStatus_t TSD_cursorFindNext(TSD_cursorHndl_t cursor, TSD_Point_t* out_point) {
	cursorHndl_t c = (cursorHndl_t) cursor;
	BPT_record_t bptRecord;
	EDB_result_t err;
	BPT_cursorStatus_t cs;
	TSD_cursorStatus_t tsdCErr;

	if (c->status == TSD_CURSOR_INITIALIZED) {
		c->status = TSD_CURSOR_ACTIVE;
	} else if (c->status == TSD_CURSOR_UNINITIALIZED) {
		return TSD_CURSOR_UNINITIALIZED;
	} else if (c->status != TSD_CURSOR_ACTIVE)
		return c->status;

	do {
		if (c->fpObj.oph == NULL) {
			cs = EDB_idx_findNext(c->xHndl, &c->bptCursor, &bptRecord);
			if (cs == BPT_CS_CURSOR_ACTIVE) {
				//LOG_INFO(TSD_LIB, "Cursor Open Value. k %d vIdx %d", c->edbCursor.bptCursor.key ,c->edbCursor.bptCursor.valueOffset)
				if ((err = FPM_objectOpen(c->xHndl, &c->fpObj, c->bptCursor.bptCursor.valueOffset))) {
					return err;
				}

			} else if (cs == BPT_CS_END_OF_RESULTS) {

				if (c->firstRange) {
					c->firstRange = false;
					if (initEDBSearchRange(c))
						continue;
				}

				return TSD_CURSOR_AT_END;
			} else {
				return TSD_CURSOR_ERROR_EDB;
			}
		} else {
			tsdCErr = cursorScanValue(cursor, out_point);
			if (tsdCErr == TSD_CURSOR_ACTIVE) {
#if TSD_ENABLE_COMBINED_PERIOD
				if (out_point->time > c->constrainedTo)
					EDB_obj_close(&c->fpObj);
				if (out_point->time >= c->constrainedFrom)
					return tsdCErr;
#else
				return tsdCErr;
#endif
			} else if (tsdCErr == TSD_CURSOR_AT_END) {
				//LOG_INFO(TSD_LIB, "Cursor Close Value")
				EDB_obj_close(&c->fpObj);

			} else {
				return tsdCErr;
			}
		}

	} while (1);
//record.valuePgIdx

//Err = EDB_openValue(tsd.edbHndl, periodKey, &currentPeriod.fpValue);

}
#endif
