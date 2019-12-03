/*
 * series.c
 *
 */

#include "config.h"
#if INCLUDE_TSD

#include "env.h"
#include "os.h"

#include <timeSeriesData/prv.h>
#include "lib/assert.h"
#include "lib/vector.h"

static TSD_SeriesId_t seriesId_getNext();

void tsd_seriesInit() {
	vec_init(&tsd.seriesVector, sizeof(seriesInfo_t), tsd.h.seriesCount);
}

TSD_SeriesId_t TSD_seriesCreate(TSD_dataTypes_e pointType) {
	TSD_Result_e err;
	seriesInfo_t nSeries;

	memset(&nSeries, 0, sizeof(seriesInfo_t));

	err = (TSD_Result_e) _OS_MUTEX_LOCK(&tsd.lock);
	_ASSERT_DEBUG(err == 0);

	//_ASSERT_DEBUG(tsd.pntTrns == NULL);
	tsd_writeXStart();

	nSeries.id = seriesId_getNext();

	nSeries.config.isPointReference = pointType == tsd_unknown;
	nSeries.config.pointType = pointType;

	if ((err = tsd_writeSeriesHeader(&nSeries))) {
		//bug
		_ASSERT_DEBUG(0);
		goto err;
	}

	vec_add(&tsd.seriesVector, &nSeries);
	tsd.h.seriesCount++;

	err = tsd_writeHeader();

	tsd_writeXEnd();

	_OS_MUTEX_UNLOCK(&tsd.lock);

	EDB_PRINT_T("Create SId %d Typ %d", nSeries.id, pointType);

	return nSeries.id;

	err:

	_OS_MUTEX_UNLOCK(&tsd.lock);
	return 0;
}

TSD_Result_e TSD_seriesDrop(TSD_SeriesId_t id) {
	seriesInfo_t* s;
	TSD_Result_e r;

	r = (TSD_Result_e) _OS_MUTEX_LOCK(&tsd.lock);
	_ASSERT_DEBUG(r == 0);

	r = TSD_DOES_NOT_EXIST;

	tsd_xEnd();

	for (uint32_t i = 0; i < tsd.seriesVector.size; i++) {
		s = VEC_GET_PTR((&tsd.seriesVector), seriesInfo_t*, i);
		if (s->id == id) {

			tsd_truncate(s, 0);

			{
				tsd_writeXStart();
				//EDB_trns_start(tsd.edbHndl, &x);

				EDB_delete(tsd.wrtTrns, tsd.srsTbl, s->id);
				tsd_writeHeader();

				//EDB_trns_commit(x);
				tsd_writeXEnd();
			}

			vec_delete(&tsd.seriesVector, i);
			tsd.h.seriesCount--;
			EDB_PRINT_T("Drop SId %d", id);
			r = TSD_OKAY;
			break;
		}
	}

	_OS_MUTEX_UNLOCK(&tsd.lock);

	return r;

}

void tsd_unLoadSeriesHeaders() {
	for (uint32_t i = tsd.seriesVector.size; i-- > 0;) {
		vec_delete(&tsd.seriesVector, i);
		tsd.h.seriesCount--;
	}
	//tsd.h.seriesCount = 0;
	_ASSERT_DEBUG(tsd.h.seriesCount == 0);
	_ASSERT_DEBUG(tsd.seriesVector.size == 0);
}

TSD_Result_e tsd_loadSeriesHeaders() {
	EDB_result_t kErr;
	BPT_Result_e bErr;
	BPT_cursor_t c;
	BPT_record_t record;
	TSD_SeriesPersistedInfo_t spi;
	buf_t value;

	value.data = (void*) &spi;
	value.dataLen = sizeof(TSD_SeriesPersistedInfo_t);

	if((bErr = EDB_idx_initSearch_Range(tsd.wrtTrns, tsd.srsTbl->rowIndex, &c, TSD_SERIES_PGIDX_START,
	TSD_SERIES_PGIDX_END, BPT_PT_ACS))){
		return (TSD_Result_e)bErr;
	}

	while (EDB_idx_findNext(tsd.wrtTrns,&c, &record) == BPT_CS_CURSOR_ACTIVE) {

		if ((kErr = EDB_get_byValuePgIdx(tsd.wrtTrns, tsd.srsTbl, record.valuePgIdx, &value))) {
			return (TSD_Result_e)kErr;
		}

		seriesInfo_t s;
		memset(&s, 0, sizeof(seriesInfo_t));
		s.id = spi.id;
		memcpy(&s.state, &spi.state, sizeof(tsd_seriesState_t));
		memcpy(&s.config, &spi.config, sizeof(tsd_seriesconfig_t));

		vec_add(&tsd.seriesVector, &s);
		//tsd.h.seriesCount++;
	}

	if (tsd.seriesVector.size != tsd.h.seriesCount) {
		/*
		 * If this happens outside of during dev
		 * Then I think something is very wrong, it should never happen
		 * A transaction should wrap each Series Add/Remove,
		 * so....
		 */
		tsd.h.seriesCount = tsd.seriesVector.size;
		EDB_PRINT_E( "TSD Inconsistency, sCount");
		//return TSD_SERIES_CORRUPTED;
	}

	return TSD_OKAY;
}

TSD_Result_e tsd_writeSeriesHeader(seriesInfo_t* s) {
	EDB_result_t kErr;
	TSD_SeriesPersistedInfo_t spi;
	buf_t value;

	_ASSERT_DEBUG(tsd.wrtTrns != NULL);
	_ASSERT_DEBUG(s->id >= TSD_SERIES_PGIDX_START);

	value.data = (void*) &spi;
	value.dataLen = sizeof(TSD_SeriesPersistedInfo_t);

	spi.id = s->id;
	memcpy(&spi.state, &s->state, sizeof(tsd_seriesState_t));
	memcpy(&spi.config, &s->config, sizeof(tsd_seriesconfig_t));

	if ((kErr = EDB_set(tsd.wrtTrns, tsd.srsTbl, s->id, &value))) {
		return (TSD_Result_e) kErr;
	}
	return TSD_OKAY;
}

seriesInfo_t* tsd_getSeries(TSD_SeriesId_t id) {
	seriesInfo_t* s;
	for (uint32_t i = 0; i < tsd.seriesVector.size; i++) {
		s = VEC_GET_PTR((&tsd.seriesVector), seriesInfo_t*, i);
		if (s->id == id)
			return s;
	}
	return NULL;
}

seriesInfo_t* tsd_getSeriesWithLargestStagedBuffer() {
	seriesInfo_t* s;
	seriesInfo_t* largestStagedS = NULL;
	for (uint32_t i = 0; i < tsd.seriesVector.size; i++) {
		s = VEC_GET_PTR((&tsd.seriesVector), seriesInfo_t*, i);
		if (largestStagedS == NULL || s->staged.count > largestStagedS->staged.count)
			largestStagedS = s;

	}
	return largestStagedS;
}


TSD_SeriesId_t seriesId_getNext() {
	EDB_result_t kErr;

	do {
		if (tsd.h.nextSeriesId == 0)
			tsd.h.nextSeriesId++;

		if (tsd.h.nextSeriesId >= TSD_SERIES_ID_RESERVED_START && tsd.h.nextSeriesId < TSD_SERIES_ID_RESERVED_END)
			tsd.h.nextSeriesId = tsd_seriesId_initValue();

		kErr = EDB_exists(tsd.wrtTrns, tsd.srsTbl, tsd.h.nextSeriesId);
		if (kErr == EDB_NOT_FOUND)
			break;

		tsd.h.nextSeriesId++;
	} while (kErr == EDB_OKAY);

	return tsd.h.nextSeriesId;
}

#endif

