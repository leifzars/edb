/*
 * rawRegion.c
 *
 */

#include <edb/config.h>
#if INCLUDE_EDB

#include <edb/pageMgr/pm_prv.h>

EDB_result_t FPM_RR_getNewRegion(EDB_Hndl_t pgMgrHndl, uint32_t pgCnt, uint32_t* out_pageIdx) {
	*out_pageIdx = pgMgrHndl->fpm.s.usedPageCount;
	pgMgrHndl->fpm.s.usedPageCount += pgCnt;

	return EDB_OKAY;
}

#endif
