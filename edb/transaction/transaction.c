/*
 * trasaction.c
 *
 */

#include <edb/config.h>
#if INCLUDE_EDB
#if EDB_ENABLE_X

#include "stdlib.h"
#include "string.h"

#include <edb/pageMgr/pm_types.h>
#include <edb/pageMgr/pm_prv.h>
#include "../wal/walIndex.h"
#include "lib/utils.h"

static void attemptReturnOfCommitedWriters(EDB_Hndl_t edbHndl);

static EDB_result_t edb_trns_new(EDB_Hndl_t edbHndl, EDB_xHndl_t parentTrns, uint32_t xId, EDB_xHndl_t* out_newTrns);
static void edb_trns_return(EDB_xHndl_t trns);
static EDB_result_t edb_trns_commitCache(EDB_xHndl_t xHndl);
static EDB_result_t edb_trns_cancelCache(EDB_xHndl_t xHndl);

EDB_result_t edb_trns_init(EDB_Hndl_t edbHndl) {
	edbHndl->xIdNext = 1;
	edbHndl->dataVersion = 1;

	edbHndl->x.cache.xUsed = false;

	vec_init(&edbHndl->x.cache.x.pageRefs, sizeof(EDB_WAL_pgStatus_t), 0);

#if EDB_ENABLED_NESTED_X
	_OS_SEMAPHORE_OPEN(&edbHndl->x.wrtSem, 1);
	dll_init(&edbHndl->x.blockedWriters, 16);
	dll_init(&edbHndl->x.allActive, 16);
	dll_init(&edbHndl->x.writersCommited, 16);
#endif

	return EDB_OKAY;
}

void edb_trns_deinit(EDB_Hndl_t edbHndl) {
#if EDB_ENABLED_NESTED_X
	_ASSERT_DEBUG(edbHndl->x.activeWriter == NULL);
	_ASSERT_DEBUG(edbHndl->x.active == NULL);
	_ASSERT_DEBUG(edbHndl->x.blockedWriters.length == 0);
	//_ASSERT_DEBUG(edbHndl->x.allActive.length == 0);

	EDB_xHndl_t x;
	dllElementPtr_t el;

	el = edbHndl->x.allActive.head;
	while ((void*) el != NULL) {
		x = (EDB_xHndl_t) el;
		el = x->n.next;
		edb_trns_return(x);
	}

	_OS_SEMAPHORE_CLOSE(&edbHndl->x.wrtSem);
#endif
}

EDB_result_t edb_trns_new(EDB_Hndl_t edbHndl, EDB_xHndl_t UNUSED(parentTrns), uint32_t xId, EDB_xHndl_t* out_newTrns) {
	EDB_x_t* nX;

#if EDB_ENABLED_NESTED_WRITE_X
	_ASSERT_DEBUG(parentTrns == NULL || parentTrns->child == NULL);
#endif

#if EDB_ENABLED_NESTED_X
	if (edbHndl->x.cache.xUsed){
		nX = _OS_MAlloc(sizeof(EDB_x_t));
		memset(nX, 0, sizeof(EDB_x_t));
	}else
#endif
	{
		_ASSERT(edbHndl->x.cache.xUsed == false);

		nX = &edbHndl->x.cache.x;
		vecOfPgStatus_t tmpV = nX->pageRefs;
		memset(nX, 0, sizeof(EDB_x_t));
		nX->pageRefs = tmpV;
		vec_clear(&nX->pageRefs);
		edbHndl->x.cache.xUsed = true;
	}


	nX->edbHndl = edbHndl;

	nX->xId = xId;
	nX->dataVersion = edbHndl->dataVersion;
	nX->state = EDB_X_STATE_ACTIVE;

#if EDB_ENABLED_NESTED_WRITE_X
	nX->child = NULL;
	if (parentTrns) {
		nX->parent = parentTrns;
		parentTrns->child = nX;
	} else {
		nX->parent = NULL;
	}
#endif

	*out_newTrns = nX;
	return EDB_OKAY;
}

void edb_trns_return(EDB_xHndl_t trns) {
	EDB_Hndl_t edbHndl;
	edbHndl = trns->edbHndl;

#if EDB_ENABLED_NESTED_X
	if (&edbHndl->x.cache.x == trns){
		edbHndl->x.cache.xUsed = false;
		vec_clear(&trns->pageRefs);
	}else {
		if (trns->isWriter) {
			vec_free(&trns->pageRefs);
		}
		_OS_Free(trns);
	}
#else
	edbHndl->x.cache.xUsed = false;
#endif

}

static void edb_trns_endWriter(EDB_xHndl_t xHndl) {
#if EDB_ENABLED_NESTED_X
	EDB_Hndl_t edbHndl;
	EDB_xHndl_t pX;
	edbHndl = xHndl->edbHndl;

	_ASSERT_DEBUG(edbHndl->x.activeWriter == xHndl);

	edbHndl->x.activeWriter = NULL;

#if EDB_ENABLE_X_DIAG
	EDB_PRINT_I("%sX%d> writer end pgCt: %d", edb->isWalActive ? "" : "WAL ", xHndl->xId, xHndl->pageRefs.size);
#endif

#if EDB_ENABLED_NESTED_WRITE_X
	if (xHndl->parent)
		xHndl->parent->child = NULL;

	pX = xHndl;
	do {
		if (pX->parent == NULL)
		break;
		pX = pX->parent;
		if (pX->isWriter) {
			edbHndl->x.activeWriter = pX;
		}
	}while (1);
	if (edbHndl->x.activeWriter == NULL)
	edbHndl->x.activeTopWriter = NULL;
	_OS_SEMAPHORE_POST(&xHndl->edbHndl->x.wrtSem);
#endif
#endif
}


EDB_result_t EDB_trns_start(EDB_Hndl_t edb, EDB_xHndl_t* out_newXHndl, EDB_trnsType_t type) {
	EDB_result_t r;

	r = edb_trns_start(edb, edb->xIdNext, out_newXHndl, type);

	edb->xIdNext++;
	return r;
}

EDB_result_t edb_trns_start(EDB_Hndl_t edb, uint32_t xId, EDB_xHndl_t* out_newXHndl, EDB_trnsType_t type) {
	EDB_result_t r;
	EDB_x_t* nX;

#if !EDB_ENABLED_NESTED_X
	_ASSERT_DEBUG(edb->x.active == NULL);
#endif
	r = edb_trns_new(edb, edb->x.active, xId, &nX);

#if EDB_ENABLED_NESTED_X
	dll_add(&edb->x.allActive, &nX->n);
#endif
	*out_newXHndl = nX;

#if EDB_ENABLE_X_DIAG
	EDB_PRINT_I("%sX%d> start", edb->isWalActive ? "" : "WAL ", nX->xId);
#endif

	if (type == EDB_X_READ_ONLY) {
		nX->isReadOnly = true;
	} else if (type == EDB_X_WRITER) {
		edb_trns_enter(nX);
		edb_trns_makeWriter(nX);
		edb_trns_exit(nX);
	}
	return r;
}

uint32_t EDB_trns_percentSpaceRemaing(EDB_Hndl_t edbHndl, EDB_xHndl_t UNUSED(xHndl)) {
	if (!edbHndl->isWalActive)
		return 0;

	uint32_t i = edb_wal_percentFull(edbHndl);
	return 100 - i;
}

EDB_result_t EDB_trns_commit(EDB_xHndl_t xHndl) {
	EDB_result_t r;
	EDB_Hndl_t edbHndl;
	edbHndl = xHndl->edbHndl;

	_ASSERT_DEBUG(edbHndl->x.active == NULL);

#if EDB_ENABLED_NESTED_WRITE_X
	_ASSERT_DEBUG(xHndl->child == NULL);
#endif

	edb_trns_enter(xHndl);

	if ((r = edb_trns_commitCache(xHndl)))
		return r;

	r = edb_trns_commitMetaData(xHndl);

	return r;
}

EDB_result_t edb_trns_commitCache(EDB_xHndl_t xHndl) {
	EDB_Hndl_t edbHndl;
	edbHndl = xHndl->edbHndl;

	if (edbHndl->x.active->isWriter) {
		edb_tables_cacheFlush(edbHndl, false); //Not needed if BPT root is saved in FPM Cache, but it is not yet

		//edb_cache_flushTransaction(edbHndl, xHndl);
		edb_pm_flush(edbHndl);
	}
	return EDB_OKAY;
}

EDB_result_t edb_trns_commitMetaData(EDB_xHndl_t xHndl) {
	EDB_result_t err;
	EDB_Hndl_t edb;
	edb = xHndl->edbHndl;

#if EDB_ENABLED_NESTED_X
	dll_remove(&edb->x.allActive, &xHndl->n);
#endif
	if (edb->x.active->isWriter) {

		edb->x.active->state = EDB_X_STATE_COMMITTING;
		edb->dataVersion = xHndl->dataVersion;

		edb_trns_endWriter(xHndl);
		edb_trns_exit(xHndl);

#if EDB_ENABLED_NESTED_WRITE_X
		if (xHndl->parent) {
			mergeChildIntoParent(xHndl);
		} else {
#endif

		if (edb->isWalActive) {
			if ((err = edb_wal_wrtXCommit(edb, xHndl))) {
				return err;
			}
		}
#if EDB_ENABLED_NESTED_WRITE_X
	}
#endif

#if EDB_ENABLE_X_DIAG
		EDB_PRINT_I("%sX%d> commit", edb->isWalActive ? "" : "WAL ", xHndl->xId);
#endif

#if EDB_ENABLED_NESTED_X
		dll_add(&edb->x.writersCommited, &xHndl->n);
#else
		edb_trns_return(xHndl);
#endif

	} else {

#if EDB_ENABLE_X_DIAG
		EDB_PRINT_I("%sX%d> end", edb->isWalActive ? "" : "WAL ", xHndl->xId);
#endif
		edb_trns_exit(xHndl);
		edb_trns_return(xHndl);
	}

#if EDB_ENABLED_NESTED_X
	attemptReturnOfCommitedWriters(edb);
#else
	edb_walIdx_appendXModedPages(edb, xHndl);
	edb_wal_evaluateForSync(edb);

#endif

	return EDB_OKAY;
}


#if EDB_ENABLED_NESTED_X
void attemptReturnOfCommitedWriters(EDB_Hndl_t edbHndl) {
	EDB_xHndl_t x;
	dllElementPtr_t el;
	EDB_xId_t oldestActiveDataVersion = 0xffffffff;
	bool atemptSync = false;

	el = edbHndl->x.allActive.head;
	while ((void*) el != NULL) {
		x = (EDB_xHndl_t) el;
		if (x->dataVersion < oldestActiveDataVersion)
			oldestActiveDataVersion = x->dataVersion;
		el = x->n.next;
	}

	atemptSync = edbHndl->x.writersCommited.length > 0;
	el = edbHndl->x.writersCommited.head;
	while ((void*) el != NULL) {
		x = (EDB_xHndl_t) el;

		el = x->n.next;

		if (x->dataVersion <= oldestActiveDataVersion) {
			edb_walIdx_appendXModedPages(edbHndl, x);
			dll_remove(&edbHndl->x.writersCommited, &x->n);
			edb_trns_return(x);
		}
	}

	if (edbHndl->isWalActive) {
		if (atemptSync && edbHndl->x.activeWriter == NULL && edbHndl->x.writersCommited.length == 0) {
			/*
			 * WAL must be fully committed, with no partial commits.
			 */
			if (edbHndl->x.prev.dataVersion != edbHndl->dataVersion) {
				/*
				 * Seems that the cache is out of sync.
				 *  The WAL Sync op assumes that the cache is synced.
				 *  So Invalidate our cache.	 *
				 */
				edb_trns__cacheSync(edbHndl);
			}

			edb_wal_evaluateForSync(edbHndl);
		}
	}
}
#endif

void edb_trns__cacheSync(EDB_Hndl_t edbHndl) {
#if EDB_ENABLED_NESTED_X
	if (edbHndl->x.activeWriter == NULL) {
		/*
		 * The thought is that for a commit to be a commit
		 * it must be flushed, in full.
		 * If the cache is out of sync, and we have no writer
		 * then we know the cache is already synced
		 */
//_ASSERT(FPM && Indexes are clean)
		/*
		 * Someone else was using the caches, not sure what is in them
		 * Invalidate them.
		 */
		edb_tables_cacheInvalidate(edbHndl);
	} else {
		/*
		 * Someone else is actively writing, the index cache and page (FPM) cache
		 * are likely of a different version, flush and clear both
		 */
		EDB_xHndl_t tmp = edbHndl->x.active;
		edbHndl->x.active = edbHndl->x.activeWriter;
		edb_tables_cacheFlush(edbHndl, true);
		//FPM_flush(edbHndl);
		edbHndl->x.active = tmp;
	}
#else
	edb_tables_cacheInvalidate(edbHndl);
#endif
	//FPM_clearCache(edbHndl);
}

EDB_result_t EDB_trns_cancel(EDB_xHndl_t xHndl) {
	EDB_result_t r;
	EDB_Hndl_t edbHndl;

	edbHndl = xHndl->edbHndl;

	_ASSERT_DEBUG(edbHndl->x.active == NULL);

	edb_trns_enter(xHndl);

	r = edb_trns_cancelCache(xHndl);
	r = edb_trns_cancelMetaData(xHndl);

	return r;
}

EDB_result_t edb_trns_cancelCache(EDB_xHndl_t xHndl) {
	EDB_result_t r;
	EDB_Hndl_t edbHndl;

	edbHndl = xHndl->edbHndl;

	r = EDB_OKAY;
	if (xHndl->isWriter) {
		fpm_cache_evictTransaction(edbHndl, xHndl);
		//cacheSync(edbHndl);
	}
	return r;
}

EDB_result_t edb_trns_cancelMetaData(EDB_xHndl_t xHndl) {
	EDB_result_t r;
	EDB_Hndl_t edb;

	edb = xHndl->edbHndl;
	r = EDB_OKAY;

#if EDB_ENABLED_NESTED_X
	dll_remove(&edb->x.allActive, &xHndl->n);
#endif
	if (xHndl->isWriter) {
		if (edb->isWalActive) {
			r = edb_wal_wrtXCancle(edb, xHndl);
		}

		//xHndl->xCommitIdEnd = edb->xCommitId;

		edb_trns_endWriter(xHndl);
	}

	edb_trns_exit(xHndl);
	edb_trns_return(xHndl);

#if EDB_ENABLE_X_DIAG
	EDB_PRINT_I("%sX%d> cancel", edb->isWalActive ? "" : "WAL ", xHndl->xId);
#endif
	return r;
}

EDB_result_t edb_trns_failed(EDB_xHndl_t xHndl) {
	EDB_result_t r;
	EDB_Hndl_t edb;

	r = EDB_OKAY;
	edb = xHndl->edbHndl;

	if (xHndl->isWriter && edb->isWalActive)
		r = edb_wal_wrtXCancle(edb, xHndl);

	edb_trns_return(xHndl);

//if (edbHndl->currentX.requiresExclusiveWriteLock)
//	_mutex_unlock(&edbHndl->xExclusiveLock);

#if EDB_ENABLE_X_DIAG
	EDB_PRINT_I("%sX>%d failed", edb->isWalActive ? "" : "WAL ", xHndl->xId);
#endif
	return r;
}

void edb_trns_makeWriter(EDB_xHndl_t xHndl) {
	EDB_Hndl_t edb;

	edb = xHndl->edbHndl;

	_ASSERT_DEBUG(xHndl != NULL);
	_ASSERT_DEBUG(!edb->isWalActive || (edb->isWalActive && edb->x.active == xHndl));
	_ASSERT_DEBUG(!xHndl->isReadOnly);
	_ASSERT_DEBUG(!xHndl->isWriter);

#if EDB_ENABLED_NESTED_X
	if (&edb->x.cache.x != xHndl || edb->x.cache.x.pageRefs.items == NULL) {
		vec_init(&xHndl->pageRefs, sizeof(EDB_WAL_pgStatus_t), EDB_X_AVG_PAGE_CNT);
	}

	xHndl->dataVersion++;
#endif

	xHndl->isWriter = true;

#if EDB_ENABLE_X_DIAG
	EDB_PRINT_I("%sX%d> writer start", edb->isWalActive ? "" : "WAL ", xHndl->xId);
#endif

#if EDB_ENABLED_NESTED_WRITE_X
	EDB_xHndl_t topX;
	topX = xHndl;

	do {
		if (topX->parent == NULL)
		break;
		topX = topX->parent;
	}while (1);

//dll_remove(&edb->x.readers, &x->n);
//dll_push(&edb->x.blockedWriters, &x->n);

	if (edb->x.activeTopWriter != topX) {
		edb_trns_exit(xHndl);
		edb->x.active = NULL;
		_OS_SEMAPHORE_WAIT(&edb->x.wrtSem);
		_ASSERT_DEBUG(edb->x.activeTopWriter == NULL);
		_ASSERT_DEBUG(edb->x.activeWriter == NULL);
		edb->x.activeTopWriter = topX;
		edb->x.active = xHndl;
	} else {
//_ASSERT_DEBUG(edb->x.activeWriter is parent to x);
	}

#endif

#if EDB_ENABLED_NESTED_X
	edb->x.activeWriter = xHndl;
#endif

	if (edb->isWalActive) {
		edb_wal_wrtXStart(edb, xHndl);
	}
}

#if EDB_ENABLED_NESTED_WRITE_X
void mergeChildIntoParent(EDB_xHndl_t childX) {
	uint32_t i;
	EDB_xHndl_t pX;
	EDB_WAL_pgStatus_t* pgsPtr;

	pX = childX->parent;

	pgsPtr = (EDB_WAL_pgStatus_t*) childX->pageRefs.items;
	i = childX->pageRefs.size;
	while (i--) {
		edb_trns_addPg(pX, pgsPtr->pgIdx, pgsPtr->walOffset);
		pgsPtr++;
	}
}
#endif

void edb_trns_addPg(EDB_xHndl_t x, pgIdx_t pgIdx, uint32_t walOffset) {
	_ASSERT_DEBUG(x != NULL);
	edb_trns_addPgToVec(&x->pageRefs, pgIdx, walOffset);
}


void edb_trns_addPgToVec(vec_t * vec, pgIdx_t pgIdx, uint32_t walOffset) {

	EDB_WAL_pgStatus_t* psPtr;

	psPtr = vec_find_binary(vec, pgIdx, offsetof(EDB_WAL_pgStatus_t, pgIdx));
	if (psPtr) {
		psPtr->walOffset = walOffset;
	} else {
		EDB_WAL_pgStatus_t ps;
		ps.pgIdx = pgIdx;
		ps.walOffset = walOffset;
		vec_add_sorted(vec, &ps, offsetof(EDB_WAL_pgStatus_t, pgIdx));
	}
}

//void merge(vec_t *vecOfPgRefsA_Into, vec_t *vecOfPgRefsB_From) {
//	vec_t* a;
//	vec_t* b;
//	vec_t c;
//	EDB_WAL_pgStatus_t* pgRefA, *pgRefB;
//	uint32_t i, j;
//
//	a = vecOfPgRefsA_Into;
//	b = vecOfPgRefsB_From;
//
//	vec_init(&c, sizeof(EDB_WAL_pgStatus_t), max(a->size,b->size));
//
//	pgRefA = (EDB_WAL_pgStatus_t*) a->items;
//	pgRefB = (EDB_WAL_pgStatus_t*) b->items;
//
//	i = 0;
//	j = 0;
//
//	while (i < a->size && j < b->size) {
//		if (pgRefA->pgIdx == pgRefB->pgIdx) {
//			vec_add(&c, &pgRefB);
//			pgRefA++;
//			pgRefB++;
//			i++;
//			j++;
//		} else if (pgRefA->pgIdx < pgRefB->pgIdx) {
//			vec_add(&c, &pgRefA);
//			pgRefA++;
//			i++;
//		} else {
//			vec_add(&c, &pgRefB);
//			pgRefB++;
//			j++;
//		}
//	}
//
//	if (i >= a->size) {
//		while (j < b->size) {
//			vec_add(&c, &pgRefB);
//			pgRefB++;
//			j++;
//		}
//	}
//
//	if (j >= b->size) {
//		while (i < a->size) {
//			vec_add(&c, &pgRefA);
//			pgRefA++;
//			i++;
//		}
//	}
//	vec_free(vecOfPgRefsA_Into);
//	memcpy(a, &c, sizeof(vec_t));
//}

#endif
#endif

