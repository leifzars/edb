/*
 * pageTypeDescriptor.c
 *
 */

#include <edb/config.h>
#if INCLUDE_EDB

#include "pageTypeDescriptor.h"

#include "fileHeader.h"
#include "freePageHeader.h"
#include "../object/objectPageHeader.h"

#include "valuePageHeader.h"

fpm_valuePageHeader_t fpm_pgTypeInfos[] = { //
		{ EDB_PM_PT_UNKNOWN, false, false, sizeof(FPM_fileHeader_t) }, //
				{ EDB_PM_PT_HEADER, false, false, sizeof(FPM_fileHeader_t) }, //
				{ EDB_PM_PT_EMPTY_ALLOCATED, false, false, 0 }, // never persisted
				{ EDB_PM_PT_FREE, false, false, sizeof(freePageHeader_t) }, //
				{ EDB_PM_PT_RAW, false, false, sizeof(edb_pm_pageHeader_t) }, //

				{ EDB_PM_PT_INDEX, false, false, sizeof(edb_pm_pageHeader_t) }, //

				{ EDB_PM_PT_OBJ_RAW, true, false, sizeof(FPM_objPgHeader_t) }, //
				{ EDB_PM_PT_OBJ_RAW_OF, true, true, sizeof(FPM_objBasePgHeader_t) }, //

				{ EDB_PM_PT_TABLE_DEF, true, false, sizeof(edb_pm_pageHeader_t) }, //
				{ EDB_PM_PT_TABLE_DEF_OVERFLOW, true, true, sizeof(edb_pm_pageHeader_t) }, //

				{ EDB_PM_PT_INDEX_DEF, true, false, sizeof(FPM_objPgHeader_t) }, //
				{ EDB_PM_PT_INDEX_DEF_OVERFLOW, true, true, sizeof(FPM_objBasePgHeader_t) }, //

				{ EDB_PM_PT_VALUE, true, false, sizeof(valuePageHeader_t) }, //
				{ EDB_PM_PT_VALUE_OVERFLOW, true, true, sizeof(valueOverFlowPageHeader_t) }, //

		//{ STORE_UNKNOWN, false, false, 0 }, //

};

#endif
