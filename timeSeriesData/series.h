/*
 * tsd_series.h
 *
 */

#ifndef TIMESERIESDATA_TSD_SERIES_H_
#define TIMESERIESDATA_TSD_SERIES_H_

#include "config.h"
#if INCLUDE_TSD

static inline TSD_SeriesId_t tsd_seriesId_initValue() {
	return  TSD_SERIES_ID_RESERVED_END;
}

#endif
#endif /* TIMESERIESDATA_TSD_SERIES_H_ */
