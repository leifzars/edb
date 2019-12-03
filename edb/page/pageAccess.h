/*
 * pageAccess.h
 *
 */

#ifndef EDB_PAGE_PAGE_ACCESS_H
#define EDB_PAGE_PAGE_ACCESS_H

#include <edb/config.h>
#if INCLUDE_EDB

#include "../pageMgr/pm_types.h"

typedef enum {
	FPM_PGBUF_NONE, FPM_PGBUF_DIRTY, FPM_PGBUF_FLUSH, FPM_PGBUF_DIRTY_ALLOCATED
} fpm_ReturnPageOptions_e;

EDB_result_t FPM_getPage(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx, bool load, void** b);

void fpm_returnPage(EDB_Hndl_t pgMgrHndl, void* pgPtr, fpm_ReturnPageOptions_e ops);
void fpm_setPageDirty(EDB_Hndl_t pgMgrHndl, void* pgPtr);
void fpm_setPageType(EDB_Hndl_t pgMgrHndl, void* pgBufPtr, pageType_e pgType);

pgIdx_t fpm_allocPage(EDB_Hndl_t pgMgrHndl);
EDB_result_t fpm_getEmptyPage(EDB_Hndl_t pgMgrHndl, pageType_e pgType, pgIdx_t* out_pgIdx, void **b);

pgIdx_t fpm_getPageIndex(EDB_Hndl_t pgMgrHndl, void* ptr);

EDB_result_t fpm_flushAll(EDB_Hndl_t pgMgrHndl);
EDB_result_t fpm_flushPage(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx);

void* fpm_p2pd(void* pgPtr);
void* fpm_pd2p(void* pgDataPtr);

bool fpm_isInCache(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx);


#if EDB_INCLUDE_PAGE_DIAG
EDB_result_t fpm_assertPageCacheUnused(EDB_Hndl_t pgMgrHndl);
EDB_result_t fpm_assertPageCacheNotModified(EDB_Hndl_t pgMgrHndl);
EDB_result_t fpm_pageCacheCountUsed(EDB_Hndl_t pgMgrHndl, uint32_t* out);
#endif



#endif
#endif /* EDB_PAGE_PAGE_ACCESS_H */
