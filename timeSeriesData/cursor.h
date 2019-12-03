/*
 * cursor.h
 *
 */

#ifndef TIMESERIESDATA_TSD_CURSOR_H_
#define TIMESERIESDATA_TSD_CURSOR_H_


#include "config.h"
#if INCLUDE_TSD
#include "env.h"
#include "tsd.h"

typedef enum {
	TSD_CURSOR_UNINITIALIZED,
	TSD_CURSOR_INITIALIZED,
	TSD_CURSOR_ACTIVE,
	TSD_CURSOR_AT_END,
	TSD_CURSOR_ERROR,
	TSD_CURSOR_ERROR_FPM,
	TSD_CURSOR_ERROR_EDB,
} TSD_cursorStatus_t;


typedef void* TSD_cursorHndl_t;


TSD_cursorHndl_t TSD_cursorInitAll(TSD_SeriesId_t seriesId, TSD_order_t order);
TSD_cursorHndl_t TSD_cursorInit_range( TSD_SeriesId_t seriesId, timeTicks_t from, timeTicks_t to, TSD_order_t order);
void TSD_cursorReturn(TSD_cursorHndl_t itr);

TSD_cursorStatus_t TSD_cursorFindNext(TSD_cursorHndl_t itr, TSD_Point_t* out_point);


#endif
#endif /* TIMESERIESDATA_TSD_CURSOR_H_ */
