/*
 * valuePageHeader.h
 *
 */

#ifndef EDB_PAGEHEADERS_FREEPAGEHEADER_H
#define EDB_PAGEHEADERS_FREEPAGEHEADER_H

#include <edb/config.h>
#if INCLUDE_EDB

#include "pageHeader.h"


typedef PRE_PACKED struct {
	edb_pm_pageHeader_t base;
	pgIdx_t nextPgIdx;

} POST_PACKED freePageHeader_t;


#endif
#endif /* EDB_PAGEHEADERS_FREEPAGEHEADER_H */
