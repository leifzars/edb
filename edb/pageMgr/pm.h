/*
 * pageManager.h
 *
 */

#ifndef EDB_PAGEMGR_PM_H
#define EDB_PAGEMGR_PM_H

#include <edb/config.h>
#if INCLUDE_EDB

#include "pm_s.h"

#include "pm_types.h"


#include "../value/valueAccess.h"
#include <edb/bTree/bpt_lib.h>

/*
 * Threaded Access: concurrent access not allowed on same handle.
 *
 * The page cache is shared between handles, and access to it is thread safe
 */

typedef struct {
	bool resetAll;
	bool resetOnCorruption;
	bool isFile;
	const char* fileName;
	pgIdx_t firstPageOffset;
	uint32_t maxPageCount;
	bool useWAL;
}edb_pm_openArgs_t, *edb_pm_openArgsPtr_t;

uint32_t edb_pm_getInstAllocSize(char* fileName);
EDB_result_t edb_pm_open(EDB_Hndl_t pgMgrHndl, edb_pm_openArgsPtr_t argsPtr);
EDB_result_t edb_pm_reLoadPMHeader(EDB_Hndl_t pgMgrHndl);
void edb_pm_close(EDB_Hndl_t pgMgrHndl);
void edb_pm_flush(EDB_Hndl_t pgMgrHndl);
void edb_pm_clearCache(EDB_Hndl_t pgMgrHndl);

uint32_t edb_pm_availablePages(EDB_Hndl_t pgMgrHndl);
uint32_t edb_pm_usedPages(EDB_Hndl_t pgMgrHndl);

void edb_pm_setMaxPageCount(EDB_Hndl_t pgMgrHndl, uint32_t count);
uint32_t edb_pm_getMaxPageCount(EDB_Hndl_t pgMgrHndl);
uint32_t edb_pm_consumedPages(EDB_Hndl_t pgMgrHndl);

pageType_e edb_pm_getPageType(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx);
EDB_result_t edb_pm_operatinalState(EDB_Hndl_t pgMgrHndl);


#if EDB_QA

void FPM_simulateOutOfSyncFileHeader(EDB_Hndl_t pgMgrHndl);
void FPM_printStatus(EDB_Hndl_t pgMgrHndl);
void FPM_QA_closeWithOutFlush(EDB_Hndl_t pgMgrHndl);
#endif

static inline void edb_pm_markDirty(EDB_Hndl_t edb) {
	edb->fpm.isHeaderDirty = true;
}



#endif
#endif /* EDB_PAGEMGR_PM_H */
