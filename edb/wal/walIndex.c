/*
 * walIndex.c
 *
 */

#include <edb/config.h>
#if INCLUDE_EDB
#if EDB_ENABLE_X

#include <edb/pageMgr/pm_prv.h>

#if EDB_WAL_INDEX_VIA_HASH
__KHASH_IMPL(pgLookUp, static kh_inline klib_unused, khint32_t, khint32_t, 1, kh_int_hash_func, kh_int_hash_equal)
#endif

void edb_walIdx_init(EDB_Hndl_t edbHndl) {
	vec_init(&edbHndl->wal.index, sizeof(EDB_WAL_pgStatus_t), EDB_WAL_AUTO_SYNC_WAL_INDEX_CT);
}

void edb_walIdx_deinit(EDB_Hndl_t edbHndl) {
	vec_free(&edbHndl->wal.index);
}

EDB_result_t edb_walIdx_find(EDB_Hndl_t edbHndl, pgIdx_t pgIdx, uint32_t* out_walOffset) {
	EDB_xHndl_t x;
	EDB_WAL_pgStatus_t* ps;

	//search my X and my parents ...
	x = edbHndl->x.active;
#if EDB_ENABLED_NESTED_WRITE_X
	while (x != NULL) {
#else
	if (x != NULL) {
#endif

		if (x->isWriter) {

			ps = vec_find_binary(&x->pageRefs, pgIdx, offsetof(EDB_WAL_pgStatus_t, pgIdx));
			if (ps) {
				*out_walOffset = ps->walOffset;
				return EDB_OKAY;
			}
		}
#if EDB_ENABLED_NESTED_WRITE_X
		x = x->parent;
#endif
	}

#if EDB_ENABLED_NESTED_X
	dllElementPtr_t el;
//search commited but not in WalIdx yet
//rev
	el = edbHndl->x.writersCommited.tail;
	while ((void*) el != NULL) {
		x = (EDB_xHndl_t) el;
		if (x->dataVersion <= edbHndl->x.active->dataVersion) {
			ps = vec_find_binary(&x->pageRefs, pgIdx, offsetof(EDB_WAL_pgStatus_t, pgIdx));
			if (ps) {
				*out_walOffset = ps->walOffset;
				return EDB_OKAY;
			}
		}
		el = x->n.prev;
	}
#endif

	ps = vec_find_binary(&edbHndl->wal.index, pgIdx, offsetof(EDB_WAL_pgStatus_t, pgIdx));
	if (!ps)
		return EDB_OKAY_DOES_NOT_EXIST;
	else
		*out_walOffset = ps->walOffset;

	return EDB_OKAY;
}

EDB_result_t edb_walIdx_addPageToX(EDB_Hndl_t edbHndl, pgIdx_t pgIdx, uint32_t walOffset) {

	edb_trns_addPg(edbHndl->x.active, pgIdx, walOffset);
	return EDB_OKAY;
}

EDB_result_t edb_walIdx_startX(EDB_Hndl_t edbHndl) {
	_ASSERT_DEBUG(edbHndl->x.active->pageRefs.size == 0);

	return EDB_OKAY;
}

void edb_walIdx_remove(EDB_Hndl_t edbHndl, uint32_t itr) {
//_ASSERT_DEBUG(edbHndl->x.active->state != EDB_X_STATE_ACTIVE);

	uint32_t idx;
	idx = vec_findIndex_binary(&edbHndl->wal.index, itr, offsetof(EDB_WAL_pgStatus_t, pgIdx));
	if (idx != VEC_NOT_FOUND) {
		vec_delete(&edbHndl->wal.index, idx);
	}

}
void edb_walIdx_purge(EDB_Hndl_t edbHndl) {
//_ASSERT_DEBUG(edbHndl->x.active->state != EDB_X_STATE_ACTIVE);

	vec_clear(&edbHndl->wal.index);

}

void walIdxAdd(EDB_Hndl_t edbHndl, pgIdx_t pgIdx, uint32_t walOffset) {
	edb_trns_addPgToVec(&edbHndl->wal.index, pgIdx, walOffset);

}

void edb_walIdx_appendXModedPages(EDB_Hndl_t edbHndl, EDB_xHndl_t x) {
	EDB_WAL_pgStatus_t* pgsPtr;
	int i;
#if EDB_ENABLE_DIAG_WAL
	uint32_t wlIdxCt;
	wlIdxCt = vec_size(&edbHndl->wal.index);
#endif
	pgsPtr = (EDB_WAL_pgStatus_t*) x->pageRefs.items;
	i = x->pageRefs.size;
	while (i--) {
		edb_trns_addPgToVec(&edbHndl->wal.index, pgsPtr->pgIdx, pgsPtr->walOffset);
		pgsPtr++;
	}

#if EDB_ENABLE_DIAG_WAL
	wlIdxCt = vec_size(&edbHndl->wal.index) - wlIdxCt;
	if (!edbHndl->isRecovering)
	EDB_PRINT_I("WAL Idx Commit #: %d Mod-pgCt: %d walIdx: %d ", x->xId, edbHndl->x.active->pageRefs.size,
			wlIdxCt);
#endif
}
uint32_t edb_walIdx_size(EDB_Hndl_t edbHndl) {
	return vec_size(&edbHndl->wal.index);
}

void edb_walIdx_start(EDB_Hndl_t edbHndl, uint32_t id) {
//	if (edbHndl->wal.currentWALCheckPoint + 1 != id)
//		return EDB_WAL_FRMT_VIOLATION;

	edbHndl->wal.currentWALIdx = id;

}
void edb_walIdx_end(EDB_Hndl_t edbHndl, uint32_t id) {
	edb_walIdx_purge(edbHndl);
}

EDB_result_t edb_walIdx_addEntry(EDB_Hndl_t edbHndl, FPM_WAL_entry_t* entry, uint32_t walOffset) {
	EDB_xHndl_t x;

	if (entry->type == FPM_WAL_PAGE) {
		if (edbHndl->x.active == NULL)
			return EDB_WAL_FRMT_VIOLATION;
		edb_trns_addPg(edbHndl->x.active, entry->page.pgIdx, walOffset);
	} else if (entry->type == FPM_WAL_X_START) {
		_ASSERT_DEBUG(edbHndl->x.active == NULL);
		if (edbHndl->x.active != NULL)
			return EDB_WAL_FRMT_VIOLATION;
		//if (edbHndl->x.active->state == EDB_X_STATE_ACTIVE)
		//	return EDB_WAL_FRMT_VIOLATION;

		edb_trns_start(edbHndl, entry->x.xId, &x, EDB_X_WRITER);
		edb_trns_enter(x);
		//edb_trns_makeWriter(x);
		//edbHndl->x.active->state = EDB_X_STATE_ACTIVE;
	} else if (entry->type == FPM_WAL_X_COMMIT) {
		if (edbHndl->x.active == NULL)
			return EDB_WAL_FRMT_VIOLATION;
		if (edbHndl->x.active->xId != entry->x.xId)
			return EDB_WAL_FRMT_VIOLATION;

		//edb_trns_exit(x);
		edb_trns_commitMetaData(edbHndl->x.active);
		//edbHndl->x.active->state = EDB_X_STATE_COMMITTING;
		//xCommit(edbHndl, entry->x.xId);
		//edbHndl->x.active->state = EDB_X_STATE_COMMITED;
	} else if (entry->type == FPM_WAL_X_CANCEL) {
		if (edbHndl->x.active == NULL)
			return EDB_WAL_FRMT_VIOLATION;

		if (entry->x.xId && entry->x.xId != edbHndl->x.active->xId)
			return EDB_WAL_FRMT_VIOLATION;

		edb_trns_exit(edbHndl->x.active);
		EDB_trns_cancel(edbHndl->x.active);
		//xCancel(edbHndl, entry->x.xId);
		//edbHndl->x.active->state = EDB_X_STATE_CANCELED;
	} else {
		return EDB_WAL_FRMT_VIOLATION;
	}
	return EDB_OKAY;
}

#endif
#endif
