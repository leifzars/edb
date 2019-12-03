/*
 * pageCache.c
 *
 */

#include <edb/config.h>
#if INCLUDE_EDB

#include "stdlib.h"
#include "string.h"

#include "pageCache_prv.h"

#include "lib/assert.h"
#include "pageIOAccess_prv.h"

EDB_result_t fpm_initBuffers(EDB_Hndl_t fpm, uint32_t number) {
	uint32_t i;
	edb_pm_pageBuf_t* blPtr;
	void* bsPtr;
	edb_pm_pageCache_t* pgCache;

	pgCache = &fpm->fpm.pgCache;

	pgCache->pageBufferCount = number;

	if ((pgCache->pageBufferList = (edb_pm_pageBuf_t*) _OS_MAlloc(number * sizeof(edb_pm_pageBuf_t))) == NULL)
		return EDB_PM_OUT_OF_MEMORY;

	if ((pgCache->pageBuffers = (void*) _OS_MAlloc(number * _OS_getPageSize())) == NULL) {
		_OS_Free(pgCache->pageBufferList);
		pgCache->pageBufferList = NULL;
		return EDB_PM_OUT_OF_MEMORY;
	}

	memset(pgCache->pageBuffers, 0, number * _OS_getPageSize());

	blPtr = pgCache->pageBufferList;
	bsPtr = pgCache->pageBuffers;

	/* initialize buflist */
	pgCache->bufList.next = blPtr;
	pgCache->bufList.prev = blPtr + (number - 1);

	for (i = 0; i < number; i++) {
		blPtr->next = blPtr + 1;
		blPtr->prev = blPtr - 1;
		blPtr->modified = false;
		blPtr->synched = false;
		blPtr->useCount = 0;
#if EDB_ENABLE_X
		blPtr->tranOwner = NULL;
#endif
		blPtr->p = bsPtr;
		bsPtr = (void *) ((char *) bsPtr + _OS_getPageSize());
		blPtr++;
	}

	pgCache->bufList.next->prev = &pgCache->bufList;
	pgCache->bufList.prev->next = &pgCache->bufList;

	return EDB_OKAY;
}

void fpm_freeBuffers(EDB_Hndl_t fpm) {
	edb_pm_pageCache_t* pgCache;

	pgCache = &fpm->fpm.pgCache;

	_OS_Free((void*) pgCache->pageBufferList);
	pgCache->pageBufferList = NULL;
	_OS_Free((void*) pgCache->pageBuffers);
	pgCache->pageBuffers = NULL;
}

void fpm_clearCache(EDB_Hndl_t fpm) {
	edb_pm_pageBuf_t *buf; /* buffer */
	edb_pm_pageCache_t* pgCache;

	pgCache = &fpm->fpm.pgCache;

	buf = pgCache->bufList.next;

	while (buf != &pgCache->bufList) {
		buf->synched = 0;
		buf = buf->next;
	}
}

static EDB_result_t flushContinuousRangeOfBuffers(EDB_Hndl_t fpm, uint32_t num, edb_pm_pgBufHndl_t* bufs) {
	EDB_result_t err;
	uint32_t j;
	//pgIdx_t startPgIdx;
	//fpm_pageCache_t* pgCache;

	//pgCache = &fpm->fpm.pgCache;

	_ASSERT_DEBUG(num > 1);

	//void* pgBufs[num];

	//startPgIdx = bufs[0]->pgIdx;

	//for (j = 0; j < num; j++) {
	//	pgBufs[j] = bufs[j]->p;
	//}

#if EDB_ENABLE_X
	if (fpm->pgAcEx.enableWrite) {
		if ((err = fpm->pgAcEx.writePgs(fpm->owner, num, bufs)))
			return err;
	} else
#endif
	{
		if ((err = fpm_writePages(fpm, num, bufs)))
			return err;
	}

	for (j = 0; j < num; j++) {
		bufs[j]->modified = false;
#if EDB_CHECK_FOR_DIRT_IN_CLEAN_PAGES
		bufs[j]->crc = _EDB_CRC32(bufs[j]->p, _OS_getPageSize(), 0xFFFFFFFF);
#endif
	}

	return EDB_OKAY;
}

static EDB_result_t fpm_flushUnusedCache(EDB_Hndl_t fpm, edb_pm_pageBuf_t* bufToFlush) {
	edb_pm_pageBuf_t * buf; /* buffer */
	EDB_result_t err;
	uint32_t bufsCount = 0;
	edb_pm_pageCache_t* pgCache;

	pgCache = &fpm->fpm.pgCache;

	edb_pm_pageBuf_t* bufs[pgCache->pageBufferCount];

	buf = pgCache->bufList.next;

	while (buf != &pgCache->bufList) {
		if (buf->modified == 1 && buf->useCount == 0) {
			bufs[bufsCount] = buf;
			bufsCount++;
		}
		buf = buf->next;
	}

	//must be at least one! Really no but for now yes
	_ASSERT_DEBUG(bufsCount > 0);

	//sort by pgIdx
	uint32_t i, j;
	for (i = 1; i < bufsCount; i++) {
		edb_pm_pageBuf_t* tmp = bufs[i];
		for (j = i; j >= 1 && tmp->pgIdx < bufs[j - 1]->pgIdx; j--)
			bufs[j] = bufs[j - 1];
		bufs[j] = tmp;
	}

	//Find Runs
	pgIdx_t pgBufIdxStart = 0;
	i = 0;
	do {
		bool endOfRun;
		if (i + 1 < bufsCount)
			endOfRun = (bufs[i]->pgIdx + 1 != bufs[i + 1]->pgIdx);
		else
			endOfRun = true;

		if (endOfRun) {
			uint32_t num = (i - pgBufIdxStart) + 1;

			if (num > 1) {
				if ((err = flushContinuousRangeOfBuffers(fpm, num, &bufs[pgBufIdxStart])))
					return err;
			} else if ((void*) bufs[pgBufIdxStart] == (void*) bufToFlush) {
				if ((err = fpm_flushBuffer(fpm, bufToFlush)))
					return err;
			} else {
				//we wont flush runs of one that are not requested to be flushed.
			}
			pgBufIdxStart = i + 1;
		}
		i++;
	} while (i < bufsCount);

	_ASSERT_DEBUG(!bufToFlush->modified);

	return EDB_OKAY;
}

EDB_result_t assignBuf(EDB_Hndl_t fpm, pgIdx_t pgIdx, edb_pm_pageBuf_t **pgPtr) {
	edb_pm_pageBuf_t *buf; /* buffer */
	EDB_result_t err; /* return code */
	edb_pm_pageBuf_t* lruUnusedBuf = NULL;
	edb_pm_pageCache_t* pgCache;

#if EDB_ENABLE_X
	uint32_t activeDataVersion;
	if (fpm->x.active) {
		activeDataVersion = fpm->x.active->dataVersion;
	} else {
		activeDataVersion = 0;//fpm->dataVersion;
	}
#endif

	pgCache = &fpm->fpm.pgCache;

	/* search for buf with matching pgIdx */
	buf = pgCache->bufList.next;
	while (buf->next != &pgCache->bufList) {
#if EDB_ENABLE_X
		if (buf->synched && buf->pgIdx == pgIdx && buf->dataVersion == activeDataVersion) {
			break;
		}
#else
		if (buf->synched && buf->pgIdx == pgIdx) {
			break;
		}
#endif

		buf = buf->next;
		if (buf->useCount == 0)
			lruUnusedBuf = buf;
	}

	_ASSERT_DEBUG(buf != NULL);
	/* either buf points to a match, or it's last one in list (LRR) */
	if (buf->synched) {

#if EDB_ENABLE_X
		if (buf->pgIdx != pgIdx || buf->dataVersion != activeDataVersion) {
#else
		if (buf->pgIdx != pgIdx) {
#endif
			if (buf->useCount) {
				buf = lruUnusedBuf;
			}

			_ASSERT_DEBUG(buf->useCount == 0);
			if (buf->modified) {

#if EDB_ENABLE_CACHE_ADV_WRITE
				if ((err = fpm_flushUnusedCache(fpm, buf)) != 0) {
#else
					if ((err = fpm_flushBuffer(fpm, buf)) != 0) {
#endif

					return err;
				}
			}

			buf->synched = false;
			buf->useCount = 0;
		}
	} else {
		buf->useCount = 0;
	}
	buf->pgIdx = pgIdx;
#if EDB_ENABLE_X
	buf->dataVersion = activeDataVersion;
	buf->tranOwner = fpm->x.active;
#endif
	buf->useCount++;
	_ASSERT_DEBUG(buf->useCount < 8);
	/* remove from current position and place at front of list */
	buf->next->prev = buf->prev;
	buf->prev->next = buf->next;
	buf->next = pgCache->bufList.next;
	buf->prev = &pgCache->bufList;
	buf->next->prev = buf;
	buf->prev->next = buf;
	*pgPtr = buf;

	return EDB_OKAY;
}

EDB_result_t fpm_cacheOperation_OnAll(EDB_Hndl_t fpm, cacheRecordOperation op, uint32_t* out) {
	EDB_result_t err; /* return code */
	edb_pm_pageBuf_t *buf; /* buffer */
	edb_pm_pageCache_t* pgCache;

	pgCache = &fpm->fpm.pgCache;

	buf = pgCache->bufList.next;

	while (buf != &pgCache->bufList) {
		if ((err = op(fpm, buf, out))) {
			return err;
		}
		buf = buf->next;
	}

	return EDB_OKAY;
}

EDB_result_t fpm_cacheOperation_OnPage(EDB_Hndl_t fpm, cacheRecordOperation op, pgIdx_t pgIdx
#if EDB_ENABLE_X
		, uint32_t dataVersion
#endif
			) {
	EDB_result_t err; /* return code */
	edb_pm_pageBuf_t *buf; /* buffer */
	edb_pm_pageCache_t* pgCache;

	pgCache = &fpm->fpm.pgCache;

	buf = pgCache->bufList.next;
	err = EDB_PM_PAGE_NOT_IN_CACHE;

	while (buf != &pgCache->bufList) {
		if (buf->pgIdx == pgIdx &&
#if EDB_ENABLE_X
				buf->dataVersion == dataVersion
#else
				1
#endif
				) {
			err = op(fpm, buf, NULL);
			break;
		}
		buf = buf->next;
	}

	return err;
}

edb_pm_pageBuf_t* pageBufToPgBufHndl(EDB_Hndl_t fpm, void* pgBufPtr) {
	edb_pm_pageBuf_t *buf; /* buffer */
	uint32_t bufferIndex;
	edb_pm_pageCache_t* pgCache;

	pgCache = &fpm->fpm.pgCache;

	bufferIndex = (uint32_t)( pgBufPtr - pgCache->pageBuffers);
	_ASSERT_DEBUG((EDB_PAGE_MASK & bufferIndex) == 0);

	bufferIndex = bufferIndex >> EDB_PAGE_BIT_POSS;

	_ASSERT_DEBUG(bufferIndex < pgCache->pageBufferCount);

	buf = pgCache->pageBufferList + bufferIndex;

	return buf;
}

#if EDB_ENABLE_X
//EDB_result_t edb_cache_flushTransaction(FPM_Hndl_t fpm, EDB_xHndl_t xHndl) {
//	EDB_result_t err; /* return code */
//	fpm_pageBuf_t *buf; /* buffer */
//	fpm_pageCache_t* pgCache;
//
//	pgCache = &fpm->fpm.pgCache;
//
//	buf = pgCache->bufList.next;
//
//	while (buf != &pgCache->bufList) {
//		if (buf->synched) {
//			if (buf->dataVersion == xHndl->dataVersion) {
//				_ASSERT_DEBUG(buf->useCount == 0);
//
//				if ((err = fpm_flushBuffer(fpm, buf)) != 0) {
//					return err;
//				}
//			} else {
//				_ASSERT_DEBUG(!buf->modified);
//			}
//		}
//		buf = buf->next;
//	}
//
//	return EDB_OKAY;
//}

void fpm_cache_evictTransaction(EDB_Hndl_t fpm, EDB_xHndl_t xHndl) {
	edb_pm_pageBuf_t *buf; /* buffer */
	edb_pm_pageCache_t* pgCache;

	pgCache = &fpm->fpm.pgCache;

	buf = pgCache->bufList.next;

	while (buf != &pgCache->bufList) {
		if (buf->synched && buf->dataVersion == xHndl->dataVersion) {
			_ASSERT_DEBUG(buf->useCount == 0);
			//_ASSERT_DEBUG(!buf->modified);
			buf->synched = false;
		}
		buf = buf->next;
	}
}

#endif
#endif
