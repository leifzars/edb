/*
 * objectPageHeader.h
 *
 */

#ifndef EDB_OBJECT_OBJECT_PAGE_HEADER_H
#define EDB_OBJECT_OBJECT_PAGE_HEADER_H

#include <edb/config.h>
#if INCLUDE_EDB

#include "objectBasePageHeader.h"


typedef PRE_PACKED struct {
	FPM_objBasePgHeader_t base;

	uint32_t objLen;

} POST_PACKED FPM_objPgHeader_t;


#endif
#endif /* EDB_OBJECT_OBJECT_PAGE_HEADER_H */
