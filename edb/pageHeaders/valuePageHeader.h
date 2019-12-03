/*
 * valuePageHeader.h
 *
 */

#ifndef EDB_PAGEHEADERS_VALUEPAGEHEADER_H
#define EDB_PAGEHEADERS_VALUEPAGEHEADER_H

#include <edb/config.h>
#if INCLUDE_EDB

#include "../object/objectPageHeader.h"
#include "pageHeader.h"
#include <edb/config.h>


#define FPM_VALUE_PAGE_DATA_SIZE	(_OS_getPageSize() - sizeof(valuePageHeader_t))
#define FPM_VALUE_PAGE_OVERFLOW_DATA_SIZE	(_OS_getPageSize() - sizeof(valueOverFlowPageHeader_t))

typedef PRE_PACKED struct {
	FPM_objPgHeader_t base;
	uint32_t key;

	pgIdx_t masterPgIdx;

} POST_PACKED valuePageHeader_t;


typedef PRE_PACKED struct {
	FPM_objBasePgHeader_t base;

} POST_PACKED valueOverFlowPageHeader_t;


#endif
#endif /* EDB_PAGEHEADERS_VALUEPAGEHEADER_H */
