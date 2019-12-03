/*
 * objectAccess_ext_prv.h
 *
 */

#ifndef EDB_OBJECT_OBJECT_ACCESS_EXT_PRV_H
#define EDB_OBJECT_OBJECT_ACCESS_EXT_PRV_H

#include <edb/config.h>
#if INCLUDE_EDB

#include "../pageMgr/pm_types.h"
#include "objectAccess_ext.h"

typedef struct {
	EDB_Hndl_t pgMgrHndl;
#if EDB_ENABLE_X
	EDB_xHndl_t xHndl;
#endif
	FPM_objPgHeader_t* oph;
	FPM_objBasePgHeader_t* activePage;
	uint32_t activePageValueOffset; // value offset of start of activePage value chunk

	uint16_t objPgHeaderSize;
	uint16_t objOfPgHeaderSize;

	uint16_t objPgDataSpace;
	uint16_t objOfPgDataSpace;

	FPM_objectCursor_t cursor;
} fpm_object_t, *fpm_objectHndl_t;

#endif

#endif /* EDB_OBJECT_OBJECT_ACCESS_EXT_PRV_H */
