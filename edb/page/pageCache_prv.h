/*
 * pageCache.h
 *
 */

#ifndef EDB_PAGE_PAGE_CACHE_PRV_H
#define EDB_PAGE_PAGE_CACHE_PRV_H

#include <edb/config.h>
#if INCLUDE_EDB

#include <edb/pageMgr/pm.h>

EDB_result_t fpm_initBuffers(EDB_Hndl_t fpm,uint32_t number);

EDB_result_t assignBuf(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx, edb_pm_pageBuf_t **b);

void fpm_freeBuffers(EDB_Hndl_t fpm);

void fpm_clearCache(EDB_Hndl_t pgMgrHndl);

edb_pm_pageBuf_t* pageBufToPgBufHndl(EDB_Hndl_t pgMgrHndl, void* pgBufPtr);


typedef EDB_result_t (*cacheRecordOperation)(EDB_Hndl_t pgMgrHndl, edb_pm_pageBuf_t *buf, uint32_t* out);

EDB_result_t fpm_cacheOperation_OnAll(EDB_Hndl_t pgMgrHndl, cacheRecordOperation op, uint32_t* out) ;
EDB_result_t fpm_cacheOperation_OnPage(EDB_Hndl_t pgMgrHndl, cacheRecordOperation op, pgIdx_t pgIdx
#if EDB_ENABLE_X
		, uint32_t dataVersion
#endif
		);

#if EDB_ENABLE_X

void fpm_cache_evictTransaction(EDB_Hndl_t fpm, EDB_xHndl_t xHndl);
#endif


#endif
#endif /* EDB_PAGE_PAGE_CACHE_PRV_H */
