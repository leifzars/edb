/*
 * pageAccess.c
 *
 */

#include <edb/config.h>
#if INCLUDE_EDB

#include "lib/assert.h"

#include <edb/pageMgr/pm_prv.h>

#include "pageIOAccess_prv.h"
#include "../pageHeaders/pageHeader.h"

#if EDB_CHECK_FOR_DIRT_IN_CLEAN_PAGES
#include "lib/crc.h"
#endif

static EDB_result_t isSynced(EDB_Hndl_t pgMgrHndl, edb_pm_pageBuf_t* buf);

pgIdx_t fpm_allocPage(EDB_Hndl_t pgMgrHndl) {
	pgIdx_t pgIdx;
	if (edb_getFreePage(pgMgrHndl, &pgIdx) == EDB_OKAY) {
		//pgMgrHndl->fpm.s.indexPageCount++;
		return pgIdx;
	}
	return 0;
}

pgIdx_t fpm_getPageIndex(EDB_Hndl_t pgMgrHndl, void* pgBufPtr) {
	edb_pm_pageBuf_t *buf; /* buffer */
	buf = pageBufToPgBufHndl(pgMgrHndl, pgBufPtr);

	_ASSERT_DEBUG(buf != NULL);

	return buf->pgIdx;
}

EDB_result_t fpm_flushBuffer(EDB_Hndl_t pgMgrHndl, edb_pm_pageBuf_t* buf) {
	EDB_result_t err;
	/* flush buffer to disk */
//buf->pgIdx != 0 &&
	if (buf->modified) {

#if EDB_ENABLE_X
		if (pgMgrHndl->pgAcEx.enableWrite) {
			if ((err = pgMgrHndl->pgAcEx.writePg(pgMgrHndl->owner, buf)))
				return err;
		} else
#endif
		{
			if ((err = fpm_writePage(pgMgrHndl, buf->pgIdx, buf->p)))
				return err;
		}

#if EDB_CHECK_FOR_DIRT_IN_CLEAN_PAGES
		buf->crc = _EDB_CRC32(buf->p, _OS_getPageSize(), 0xFFFFFFFF);
#endif
		buf->modified = false;
	}
	return EDB_OKAY;
}

#include "../pageHeaders/valuePageHeader.h"
void fpm_returnPage(EDB_Hndl_t pgMgrHndl, void* pgPtr, fpm_ReturnPageOptions_e ops) {
	edb_pm_pageBuf_t* buf;
	edb_pm_pageHeader_t* ph;
	EDB_result_t err;

	ph = pgPtr;
	buf = pageBufToPgBufHndl(pgMgrHndl, pgPtr);

	_ASSERT_DEBUG(buf->useCount);
	if (ops) {
		_ASSERT_DEBUG(ph->pageNumber == buf->pgIdx);
		_ASSERT_DEBUG(ph->type != EDB_PM_PT_EMPTY_ALLOCATED || ops == FPM_PGBUF_DIRTY_ALLOCATED);

#if !IS_PROD_MODE
		valuePageHeader_t* vph = (valuePageHeader_t*) ph;
		if (vph->base.base.base.type == EDB_PM_PT_VALUE
				&& (vph->base.base.nextPage == 0 && vph->base.objLen >= (_OS_getPageSize() - sizeof(valuePageHeader_t)))) {
			_ASSERT_DEBUG(0);
		}
#endif
		buf->synched = true;

#if EDB_ENABLE_X
		if (pgMgrHndl->x.active) {
			if (!pgMgrHndl->x.active->isWriter) {
				edb_trns_makeWriter(pgMgrHndl->x.active);
			}
			if (ops) {
				buf->tranOwner = pgMgrHndl->x.active;
				buf->dataVersion = pgMgrHndl->x.active->dataVersion;
			}
		} else {
			buf->tranOwner = NULL;
			buf->dataVersion = 0;
		}
//		if (!pgMgrHndl->isWalSyncing && !pgMgrHndl->x.active->isWriter) {
//			edb_trns_makeWriter(pgMgrHndl->x.active);
//		}
#endif

		if (ops == FPM_PGBUF_FLUSH) {
			buf->modified = true;
			_ASSERT_DEBUG(buf->useCount == 1);
			if ((err = fpm_flushBuffer(pgMgrHndl, buf))) {
				_ASSERT(err);
			}
		} else if (ops == FPM_PGBUF_DIRTY || ops == FPM_PGBUF_DIRTY_ALLOCATED) {
			buf->modified = true;
#if EDB_CHECK_FOR_DIRT_IN_CLEAN_PAGES
			buf->crc = _EDB_CRC32(buf->p, _OS_getPageSize(), 0xFFFFFFFF);
#endif
		}
	} else {
		//Check for dirt
#if EDB_CHECK_FOR_DIRT_IN_CLEAN_PAGES
		uint32_t crc;
		crc = _EDB_CRC32(buf->p, _OS_getPageSize(), 0xFFFFFFFF);
		_ASSERT_DEBUG(buf->crc == crc);
#endif
	}

	buf->useCount--;
}

void fpm_setPageDirty(EDB_Hndl_t pgMgrHndl, void* pgPtr) {
	edb_pm_pageBuf_t* buf;
	edb_pm_pageHeader_t* ph;

	ph = pgPtr;
	buf = pageBufToPgBufHndl(pgMgrHndl, pgPtr);

	_ASSERT_DEBUG(ph->type != EDB_PM_PT_EMPTY_ALLOCATED);
	_ASSERT_DEBUG(ph->pageNumber == buf->pgIdx);
	_ASSERT_DEBUG(buf->useCount);
	_ASSERT_DEBUG(buf->useCount == 1);

#if EDB_ENABLE_X
	if (pgMgrHndl->x.active) {
		if (!pgMgrHndl->x.active->isWriter) {
			edb_trns_makeWriter(pgMgrHndl->x.active);
		}
		buf->tranOwner = pgMgrHndl->x.active;
		buf->dataVersion = pgMgrHndl->x.active->dataVersion;

	} else {
		buf->tranOwner = NULL;
		buf->dataVersion = 0;
	}
#endif

	buf->synched = true;
	buf->modified = true;

#if EDB_CHECK_FOR_DIRT_IN_CLEAN_PAGES
	buf->crc = _EDB_CRC32(buf->p, _OS_getPageSize(), 0xFFFFFFFF);
#endif
}

EDB_result_t fpm_getEmptyPage(EDB_Hndl_t pgMgrHndl, pageType_e pgType, pgIdx_t* out_pgIdx, void **pgPtr) {
	pgIdx_t pgIdx;
	edb_pm_pageBuf_t *buf;
	EDB_result_t err;
	edb_pm_pageHeader_t* ph;

	if ((err = edb_getFreePage(pgMgrHndl, &pgIdx))) {
		return err;
	}

	if ((err = assignBuf(pgMgrHndl, pgIdx, &buf)) != 0) {
		return err;
	}
	memset(buf->p, 0, _OS_getPageSize());

	buf->synched = true;
	ph = (edb_pm_pageHeader_t*) buf->p;
	ph->pageNumber = pgIdx;
	ph->type = pgType;

	*pgPtr = buf->p;
	*out_pgIdx = pgIdx;

	return EDB_OKAY;
}

EDB_result_t FPM_getPage(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx, bool load, void **pgPtr) {
	/* read data into buf */

	edb_pm_pageBuf_t *buf; /* buffer */
	EDB_result_t err;

	*pgPtr = NULL;

	if ((err = assignBuf(pgMgrHndl, pgIdx, &buf)) != 0) {
		return err;
	}
	if (load) {
		if (!buf->synched) {

			if (pgIdx >= pgMgrHndl->fpm.s.usedPageCount)
				return edb_handleError(pgMgrHndl, EDB_PM_ERROR_PAGE_DOES_NOT_EXIST);

#if EDB_ENABLE_X
			if (pgMgrHndl->pgAcEx.readPg) {
				if ((err = pgMgrHndl->pgAcEx.readPg(pgMgrHndl->owner, pgIdx, (void*) buf->p))) {
					if (err != EDB_OKAY_DOES_NOT_EXIST) {
						fpm_returnPage(pgMgrHndl, buf->p, FPM_PGBUF_NONE);
						return err;
					}
				}

			} else
#endif
			{
				err = EDB_OKAY_DOES_NOT_EXIST;
			}

			if (err == EDB_OKAY_DOES_NOT_EXIST) {
				if ((err = fpm_readPage(pgMgrHndl, pgIdx, (void*) buf->p))) {
					fpm_returnPage(pgMgrHndl, buf->p, FPM_PGBUF_NONE);
					return err;
				}
			}

#if EDB_CHECK_FOR_DIRT_IN_CLEAN_PAGES
			buf->crc = _EDB_CRC32(buf->p, _OS_getPageSize(), 0xFFFFFFFF);
#endif

			buf->modified = false;
			buf->synched = true;
		}
	} else {
		buf->synched = true;
		if (pgIdx >= pgMgrHndl->fpm.s.usedPageCount)
			pgMgrHndl->fpm.s.usedPageCount = pgIdx + 1;
	}

	*pgPtr = buf->p;
	return err;
}

EDB_result_t fpm_flushAll(EDB_Hndl_t pgMgrHndl) {
	return fpm_cacheOperation_OnAll(pgMgrHndl, (cacheRecordOperation)fpm_flushBuffer, NULL);
}

EDB_result_t fpm_flushPage(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx) {
	return fpm_cacheOperation_OnPage(pgMgrHndl, (cacheRecordOperation)fpm_flushBuffer, pgIdx
#if EDB_ENABLE_X
			, pgMgrHndl->dataVersion
#endif
			);
}

void* fpm_p2pd(void* pgPtr) {
	return (void*) ((uint8_t*) pgPtr + sizeof(edb_pm_pageHeader_t));
}
void* fpm_pd2p(void* pgDataPtr) {
	return (void*) ((uint8_t*) pgDataPtr - sizeof(edb_pm_pageHeader_t));
}

EDB_result_t isSynced(EDB_Hndl_t pgMgrHndl, edb_pm_pageBuf_t* buf) {
	return buf->synched ? EDB_OKAY : EDB_OKAY_DOES_NOT_EXIST;
}

bool fpm_isInCache(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx) {
	return fpm_cacheOperation_OnPage(pgMgrHndl, (cacheRecordOperation)isSynced, pgIdx
#if EDB_ENABLE_X
			, pgMgrHndl->dataVersion
#endif
			) == EDB_OKAY;
}

#if EDB_INCLUDE_PAGE_DIAG
static EDB_result_t assertPageCacheNotModified(EDB_Hndl_t UNUSED(pgMgrHndl), edb_pm_pageBuf_t* buf, uint32_t* out) {
	if (buf->fpm.synched) {
		_ASSERT(buf->modified == 0);
	}
	return EDB_OKAY;
}
static EDB_result_t assertPageCacheUnused(EDB_Hndl_t UNUSED(pgMgrHndl), edb_pm_pageBuf_t* buf, uint32_t* out) {
	if (buf->fpm.synched) {
		_ASSERT(buf->useCount == 0);
	}
	return EDB_OKAY;
}
static EDB_result_t countUsedPages(EDB_Hndl_t UNUSED(pgMgrHndl), edb_pm_pageBuf_t* buf, uint32_t* out) {
	if(buf->useCount > 0)
	(*out) ++;
	return EDB_OKAY;
}
EDB_result_t fpm_assertPageCacheUnused(EDB_Hndl_t pgMgrHndl) {
	return fpm_cacheOperation_OnAll(pgMgrHndl, assertPageCacheUnused, NULL);
}
EDB_result_t fpm_assertPageCacheNotModified(EDB_Hndl_t pgMgrHndl) {
	return fpm_cacheOperation_OnAll(pgMgrHndl, assertPageCacheNotModified, NULL);
}
EDB_result_t fpm_pageCacheCountUsed(EDB_Hndl_t pgMgrHndl, uint32_t* out) {
	return fpm_cacheOperation_OnAll(pgMgrHndl, countUsedPages, out);
}
#endif
#endif
