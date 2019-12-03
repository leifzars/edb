/*
 * header.h
 *
 */

#ifndef TIMESERIESDATA_TSD_HEADER_H_
#define TIMESERIESDATA_TSD_HEADER_H_

#include "config.h"
#if INCLUDE_TSD

#include "tsd.h"

void tsd_initHeader();
TSD_Result_e tsd_writeHeader();
TSD_Result_e tsd_readHeader();

#endif
#endif /* TIMESERIESDATA_TSD_HEADER_H_ */
