/*
 * objectPageHeader.h
 *
 */

#ifndef EDB_OBJECT_OBJECT_BASE_PAGE_HEADER_H
#define EDB_OBJECT_OBJECT_BASE_PAGE_HEADER_H


#include <edb/config.h>
#if INCLUDE_EDB

#include "../pageHeaders/pageHeader.h"

typedef PRE_PACKED struct {
	edb_pm_pageHeader_t base;

	pgIdx_t ownerPage;
	pgIdx_t nextPage;
	pgIdx_t prevPage;

} POST_PACKED FPM_objBasePgHeader_t;



#endif
#endif /* EDB_OBJECT_OBJECT_BASE_PAGE_HEADER_H */
