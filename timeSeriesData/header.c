/*
 * header.c
 *
 */

#include <timeSeriesData/prv.h>

#if INCLUDE_TSD

#include "env.h"
#include "os.h"

void tsd_initHeader(){
	tsd.h.version = TSD_VERSION;
	tsd.h.maxAge = TSD_DEFAULT_MAX_AGE_TICKS;
	tsd.h.nextSeriesId = tsd_seriesId_initValue();
	tsd.h.seriesCount = 0;
}

TSD_Result_e tsd_writeHeader() {
	TSD_Result_e err;
	buf_t value;

	value.data = (void*) &tsd.h;
	value.dataLen = sizeof(TSD_HistorianHeader_t);

	if ((err = EDB_set(tsd.wrtTrns, tsd.srsTbl, 0, &value))) {
		return (TSD_Result_e) err;
	}

	return TSD_OKAY;
}

TSD_Result_e tsd_readHeader() {
	TSD_Result_e err;
	buf_t value;
	TSD_HistorianHeader_t hh;

	value.data = (void*) &hh;
	value.dataLen = sizeof(TSD_HistorianHeader_t);

	if ((err = EDB_get(tsd.wrtTrns, tsd.srsTbl, 0, &value))) {
		return (TSD_Result_e) err;
	}

	memcpy(&tsd.h, &hh, sizeof(TSD_HistorianHeader_t));

	_ASSERT_DEBUG(tsd.h.version == TSD_VERSION);

	return TSD_OKAY;
}

#endif
