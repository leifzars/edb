/*
 * wal.c
 *
 */

#include <edb/config.h>
#if INCLUDE_EDB
#if EDB_ENABLE_X

#include "stdlib.h"
#include "../../os.h"
#include <edb/pageMgr/pm_prv.h>
#include "walIndex.h"
#include "wal_prv.h"

static EDB_result_t walCreate(EDB_Hndl_t edbHndl, uint32_t pgCnt);
static EDB_result_t walLoad(EDB_Hndl_t edbHndl);
static EDB_result_t edb_wal_wrtHeader(EDB_Hndl_t edbHndl);

static bool syncWALToDBNeeded(EDB_Hndl_t edbHndl);
static EDB_result_t syncWALToDB(EDB_Hndl_t edbHndl, bool forRecovery);
static void resetWAL(EDB_Hndl_t edbHndl);
static uint32_t edb_wal_usedPgCt(EDB_Hndl_t edbHndl);

EDB_result_t edb_wal_init(EDB_Hndl_t edbHndl) {
	EDB_result_t err;

	edb_walIdx_init(edbHndl);

	edbHndl->owner = (void*) edbHndl;
	edbHndl->pgAcEx.writePg = (edb_pm_pageWrite_ex) edb_wal_writePage_fromFPM;
	edbHndl->pgAcEx.writePgs = (edb_pm_pagesWrite_ex) edb_wal_writePages_fromFPM;
	edbHndl->pgAcEx.readPg = (edb_pm_pageRead_ex) edb_wal_readPage_fromFPM;

#if EDB_INCLUDE_EXT_WAL_DIAG
	err = fpm_assertPageCacheUnused(edbHndl);
	err = fpm_assertPageCacheNotModified(edbHndl);
#endif

	edb_pm_clearCache(edbHndl);

	edbHndl->wal.appendBuffer = _OS_MAlloc(_OS_getPageSize() * 1);
	edbHndl->wal.readBuffer = _OS_MAlloc(_OS_getPageSize() * 2);

	edbHndl->isWalRecovering = true;
	err = walLoad(edbHndl);
	if (err == EDB_NOT_FOUND) {
		if ((err = walCreate(edbHndl, EDB_WAL_MAX_PG_CT)))
			return err;
		//} else if (err == EDB_CORRUPTED) {
		//	edb_wal_wrtHeader(edbHndl);
	} else if (err == EDB_WAL_DIRTY) {
		syncWALToDB(edbHndl, true);
	} else if (err) {
		return err;
	}

	resetWAL(edbHndl);

	edbHndl->isWalRecovering = false;

	_ASSERT(edbHndl->x.active == NULL);
	edbHndl->pgAcEx.enableWrite = true;
	edbHndl->isWalActive = true;
#if EDB_ENABLE_DIAG_WAL
	EDB_PRINT_I("WAL Enabled DB@ %d WAL@ %d", edbHndl->fpm.s.walSyncIndx, edbHndl->wal.currentWALIdx);
#endif
	return err;
}
void edb_wal_deinit(EDB_Hndl_t edbHndl) {
	edb_walIdx_deinit(edbHndl);

	if (edbHndl->wal.appendBuffer)
		_OS_Free(edbHndl->wal.appendBuffer);
	if (edbHndl->wal.readBuffer)
		_OS_Free(edbHndl->wal.readBuffer);

}

void edb_wal_evaluateForSync(EDB_Hndl_t edbHndl){
	if (syncWALToDBNeeded(edbHndl)) {
		 syncWALToDB(edbHndl, false);
	}
}
EDB_result_t edb_wal_sync(EDB_Hndl_t edbHndl) {
	if (!edbHndl->isWalActive)
		return EDB_WAL_NOT_ENABLED;

	return syncWALToDB(edbHndl, false);
}

/*
 * 0-100
 */
uint32_t edb_wal_usedPgCt(EDB_Hndl_t edbHndl) {
	return edbHndl->wal.activeWritePgIdx - edbHndl->wal.startPgIdx + (edbHndl->wal.activeWritePgPos ? 1 : 0);
}

uint32_t edb_wal_percentFull(EDB_Hndl_t edbHndl) {
	return (edb_wal_usedPgCt(edbHndl) * 100) / EDB_WAL_MAX_PG_CT;
}

bool syncWALToDBNeeded(EDB_Hndl_t edbHndl) {
	if (edb_wal_usedPgCt(edbHndl) >= EDB_WAL_AUTO_SYNC_PG_CT)
		return true;
#if EDB_WAL_INDEX_VIA_HASH
	if (edbHndl->wal.indexLookUp->n_occupied > EDB_WAL_AUTO_SYNC_WAL_INDEX_CT)
	return true;
#else
	if (vec_size(&edbHndl->wal.index) > EDB_WAL_AUTO_SYNC_WAL_INDEX_CT)
		return true;

#endif

	return false;
}
EDB_result_t syncWALToDB(EDB_Hndl_t edbHndl, bool forRecovery) {
	pgIdx_t pgIdx;
	uint32_t UNUSED(walOffset);
	void* pgPtr;

#if EDB_ENABLED_NESTED_X
	_ASSERT_DEBUG(edbHndl->x.writersCommited.length == 0);
#endif
	_ASSERT_DEBUG(edbHndl->x.active == NULL);
	_ASSERT_DEBUG(edbHndl->wal.appedBufferDirty == false);

	edbHndl->pgAcEx.enableWrite = false;
	edbHndl->isWalSyncing = true;

	if (edb_walIdx_size(edbHndl)) {
#if EDB_ENABLE_DIAG_WAL
		EDB_PRINT_I("WAL Sync DB@ %d WAL@ %d  pgCt: %d", edbHndl->fpm.s.walSyncIndx, edbHndl->wal.currentWALIdx,
				edb_wal_usedPgCt(edbHndl));
#endif
		int inCache = 0;
		int fromWAL = 0;
#if EDB_WAL_INDEX_VIA_HASH
		khiter_t k;
		for (k = kh_begin(edbHndl->wal.indexLookUp); k != kh_end(edbHndl->wal.indexLookUp); ++k) {
			if (!kh_exist(edbHndl->wal.indexLookUp, k))
			continue;

			pgIdx = kh_key(edbHndl->wal.indexLookUp, k);
			walOffset = kh_val(edbHndl->wal.indexLookUp, k);
#else

		for (int i = 0; i < edbHndl->wal.index.size; i++) {
			EDB_WAL_pgStatus_t* ps = vec_get(&edbHndl->wal.index, i);

			pgIdx = ps->pgIdx;
			walOffset = ps->walOffset;

			if (pgIdx == FPM_HEADER_PG_IDX && !forRecovery) {
				/*The fpm.s or cached header is current
				 * And it will be flushed at the end of the
				 * sync, so redunant and skipped.
				 * Unless we are recovering!
				 */
#if EDB_WAL_INDEX_VIA_HASH
				edb_walIdx_remove(edbHndl, k);
#else
				edb_walIdx_remove(edbHndl, pgIdx);
#endif
				continue;
			}

#endif
			if (fpm_isInCache(edbHndl, pgIdx)) {
				FPM_getPage(edbHndl, pgIdx, true, &pgPtr);

				fpm_returnPage(edbHndl, pgPtr, FPM_PGBUF_DIRTY);

#if EDB_WAL_INDEX_VIA_HASH
				edb_walIdx_remove(edbHndl, k);
#else
				edb_walIdx_remove(edbHndl, pgIdx);
#endif
				inCache++;
			}
		}

#if EDB_WAL_INDEX_VIA_HASH
		for (k = kh_begin(edbHndl->wal.indexLookUp); k != kh_end(edbHndl->wal.indexLookUp); ++k) {
			if (!kh_exist(edbHndl->wal.indexLookUp, k))
			continue;

			pgIdx = kh_key(edbHndl->wal.indexLookUp, k);
			walOffset = kh_val(edbHndl->wal.indexLookUp, k);
#else

		for (int i = 0; i < edbHndl->wal.index.size; i++) {
			EDB_WAL_pgStatus_t* ps = vec_get(&edbHndl->wal.index, i);

			pgIdx = ps->pgIdx;
			walOffset = ps->walOffset;
#endif

#if EDB_ENABLE_DIAG_WAL
			EDB_PRINT_I("WAL Sync pgIdx %d, walPos %d", pgIdx, walOffset);
#endif

			FPM_getPage(edbHndl, pgIdx, true, &pgPtr);

			fpm_returnPage(edbHndl, pgPtr, FPM_PGBUF_DIRTY);
			fromWAL++;
		}
		edb_walIdx_purge(edbHndl);

#if EDB_ENABLE_DIAG_WAL
		EDB_PRINT_I("WAL DB Synced DB@ %d   frmCache %d frmWAL %d", edbHndl->fpm.s.walSyncIndx, inCache, fromWAL);
#endif
	}

	if (forRecovery) {
		/*
		 * We most certainly recovered a header.
		 * So we need to load it and replace our cahced copy
		 */
		edb_pm_reLoadPMHeader(edbHndl);
	}else{
	}
	edbHndl->fpm.s.walSyncIndx = edbHndl->wal.currentWALIdx;
	resetWAL(edbHndl); //also marks MasterPage Dirty

	edb_pm_flush(edbHndl);

	edbHndl->pgAcEx.enableWrite = true;
	edbHndl->isWalSyncing = false;

	return EDB_OKAY;
}

void resetWAL(EDB_Hndl_t edbHndl) {
	_ASSERT_DEBUG(!edbHndl->wal.appedBufferDirty);

	edbHndl->wal.activeWritePgIdx = edbHndl->wal.startPgIdx;
	edbHndl->wal.activeWritePgPos = 0;
	edbHndl->wal.activeWritePos = 0;

	edbHndl->wal.currentWALIdx = edbHndl->fpm.s.walSyncIndx + 1;
	edb_pm_markDirty(edbHndl);
}

EDB_result_t edb_wal_wrtHeader(EDB_Hndl_t edbHndl) {
	EDB_result_t err;
	FPM_WAL_entry_t entry;
	uint32_t salt;

	salt = rand();

	entry.type = FPM_WAL_HEADER;
	entry.header.next = edbHndl->wal.currentWALIdx;
	entry.args[1] = 0;

	if (edbHndl->wal.salt == salt)
		edbHndl->wal.salt++;
	else
		edbHndl->wal.salt = salt;

	edbHndl->wal.cumulativeCRC = 0xFFFFFFFF;

	_ASSERT_DEBUG(edbHndl->wal.activeWritePgIdx == edbHndl->wal.startPgIdx);
	_ASSERT_DEBUG(edbHndl->wal.activeWritePos == 0);
	_ASSERT_DEBUG(edbHndl->wal.activeWritePgPos == 0);

	err = edb_wal_appendEntry(edbHndl, &entry);

#if EDB_ENABLE_DIAG_WAL
	EDB_PRINT_I("WAL Start #: %d", entry.header.next);
#endif
	return err;
}

EDB_result_t edb_wal_wrtXStart(EDB_Hndl_t edbHndl, EDB_xHndl_t xHndl) {
	EDB_result_t err;
	FPM_WAL_entry_t entry;

	entry.type = FPM_WAL_X_START;
	entry.x.xId = xHndl->xId;
	entry.args[1] = 0;

	if (edbHndl->wal.activeWritePos == 0) {
		resetWAL(edbHndl);
		edb_wal_io_resetCache(edbHndl);
		edb_wal_wrtHeader(edbHndl);
	}

	if (edbHndl->fpm.s.walSyncIndx == edbHndl->wal.currentWALIdx) {
		//start new CP
	}

	err = edb_wal_appendEntry(edbHndl, &entry);
	return err;
}

EDB_result_t edb_wal_wrtXCommit(EDB_Hndl_t edbHndl, EDB_xHndl_t xHndl) {
	EDB_result_t err;
	FPM_WAL_entry_t entry;

	entry.type = FPM_WAL_X_COMMIT;
	entry.x.xId = xHndl->xId;
	entry.args[1] = 0;

	err = edb_wal_appendEntry(edbHndl, &entry);
	err = edb_wal_io_flush(edbHndl);
	//err = edb_walIdx_commitX(edbHndl);


	return err;
}

EDB_result_t edb_wal_wrtXCancle(EDB_Hndl_t edbHndl, EDB_xHndl_t xHndl) {
	EDB_result_t err;
	FPM_WAL_entry_t entry;

	entry.type = FPM_WAL_X_CANCEL;
	entry.x.xId = xHndl->xId;
	entry.args[1] = 0;

	err = edb_wal_appendEntry(edbHndl, &entry);

	/*
	 * We don't need to flush,
	 * but is simplifies things
	 */
	err = edb_wal_io_flush(edbHndl);


	//err = edb_walIdx_cancelX(edbHndl);
	return err;
}

EDB_result_t edb_wal_appendEntry(EDB_Hndl_t edbHndl, FPM_WAL_entry_t* entry) {
	EDB_result_t err;

	_ASSERT_DEBUG(
			(entry->type == FPM_WAL_HEADER && edbHndl->wal.activeWritePgIdx == edbHndl->wal.startPgIdx
					&& edbHndl->wal.activeWritePgPos == 0 && edbHndl->wal.activeWritePos == 0)
					|| (entry->type != FPM_WAL_HEADER && edbHndl->wal.activeWritePos < edbHndl->wal.size));

	entry->salt = edbHndl->wal.salt;

	entry->crc = __CRC32((const char*) entry, offsetof(FPM_WAL_entry_t, crc), edbHndl->wal.cumulativeCRC);
	edbHndl->wal.cumulativeCRC = entry->crc;
	//edbHndl->wal.cumulativeCRC = _EDB_CRC32((uint8_t*) &entry->crc, sizeof(uint32_t),
	//		edbHndl->wal.cumulativeCRC);

	err = edb_wal_io_write(edbHndl, (void*) entry, sizeof(FPM_WAL_entry_t));

	return err;
}

EDB_result_t walCreate(EDB_Hndl_t edbHndl, uint32_t pgCnt) {
	EDB_result_t err;
	pgIdx_t walStrart;

	if ((err = FPM_RR_getNewRegion(edbHndl, pgCnt, &walStrart)))
		return err;

	edbHndl->wal.startPgIdx = walStrart;

	edbHndl->wal.size = pgCnt * _OS_getPageSize();
	edbHndl->wal.sizePgCt = pgCnt;

	edbHndl->wal.activeWritePgIdx = edbHndl->wal.startPgIdx;

#if EDB_ENABLE_DIAG_WAL
	EDB_PRINT_I("WAL creating. pgIdx: %d, len: %d", edbHndl->wal.startPgIdx, edbHndl->wal.size);
#endif

	//extend File now, so not to delay writes later
	if (edbHndl->isIOBlockMode) {

	} else {	//File
		uint8_t buf[_OS_getPageSize()];
		pgIdx_t pgIdx;
		memset(buf, 0, _OS_getPageSize());
		//write the last page of the WAL
		pgIdx = edbHndl->wal.startPgIdx + pgCnt - 1;
		err = fpm_writeRawPage(edbHndl, pgIdx, (void*) buf, 1);
	}

	edbHndl->fpm.s.walSyncIndx = 0;
	edb_pm_markDirty(edbHndl);
	edbHndl->wal.currentWALIdx = 1;
	edb_setResourcePgIdx(edbHndl, FPM_R_WAL, walStrart);

#if EDB_ENABLE_DIAG_WAL
	EDB_PRINT_I("WAL created", 0);
#endif

	return err;
}

bool checkEntryCRC(EDB_Hndl_t edbHndl, FPM_WAL_entry_t* entry) {
	uint32_t crc;
	bool valid;
	crc = __CRC32((const char*) entry, offsetof(FPM_WAL_entry_t, crc), edbHndl->wal.cumulativeCRC);
	edbHndl->wal.cumulativeCRC = crc;
	valid = entry->crc == crc;

	//if (valid)
	//	crc = _EDB_CRC32((uint8_t*) &crc, sizeof(uint32_t), edbHndl->wal.cumulativeCRC);
	return valid;
}

void cumulateBlockCRC(EDB_Hndl_t edbHndl, void* buf, uint32_t len) {
	uint32_t crc;
	crc = __CRC32(buf, len, edbHndl->wal.cumulativeCRC);
	edbHndl->wal.cumulativeCRC = crc;
}

EDB_result_t walLoad(EDB_Hndl_t edbHndl) {
	EDB_result_t err;
	uint8_t buff[_OS_getPageSize()];
	uint32_t walEndLoc;
	uint32_t maxPgIdx;
	FPM_WAL_entry_t header;
	FPM_WAL_entry_t entry;

	bool aheadOfDB;
	uint32_t UNUSED(walOffset);

	uint32_t recoveredXCt = 0;
	uint32_t recoveredPgCt = 0;
	uint32_t xPgCt = 0;

	_ASSERT_DEBUG(edbHndl->tables.length == 0);

	edbHndl->wal.startPgIdx = edb_getResourcePgIdx(edbHndl, FPM_R_WAL);
	if (edbHndl->wal.startPgIdx == 0)
		return EDB_NOT_FOUND;

	maxPgIdx = 0;

	edbHndl->wal.size = EDB_WAL_MAX_PG_CT * _OS_getPageSize();
	edbHndl->wal.cumulativeCRC = 0xFFFFFFFF;

	walOffset = 0;
	walEndLoc = edbHndl->wal.size;

	_ASSERT(edbHndl->x.active == NULL);

	if ((err = edb_wal_read(edbHndl, walOffset, (void*) &header, sizeof(FPM_WAL_entry_t))))
		return err;
	walOffset += sizeof(FPM_WAL_entry_t);

	if (header.type != FPM_WAL_HEADER)
		return EDB_WAL_DIRTY;

	if (!checkEntryCRC(edbHndl, &header))
		return EDB_WAL_DIRTY;

	aheadOfDB = header.header.next > edbHndl->fpm.s.walSyncIndx;

	edb_walIdx_start(edbHndl, header.header.next);

	do {

		if ((err = edb_wal_read(edbHndl, walOffset, (void*) &entry, sizeof(FPM_WAL_entry_t))))
			return err;
		walOffset += sizeof(FPM_WAL_entry_t);

		if (entry.salt != header.salt) {
			err = EDB_WAL_DIRTY;
			break;
		}
		if (!checkEntryCRC(edbHndl, &entry)) {
			err = EDB_WAL_DIRTY;
			break;
		}

		if (entry.type == FPM_WAL_CHECK_POINT) {
			if (edbHndl->x.active == NULL) {
				err = EDB_WAL_FRMT_VIOLATION;
				break;
			}

			syncWALToDB(edbHndl, false);
			edb_walIdx_end(edbHndl, header.header.next - 1);
			edb_walIdx_start(edbHndl, header.header.next);

			aheadOfDB = entry.chkPnt.next > edbHndl->fpm.s.walSyncIndx;

		} else {
			if ((err = edb_walIdx_addEntry(edbHndl, &entry, walOffset)))
				break;

			if (entry.type == FPM_WAL_PAGE) {
				if (entry.page.pgIdx > maxPgIdx)
					maxPgIdx = entry.page.pgIdx;
				//FPM_getPage(edbHndl, pgIdx, true, &pgPtr);
				if ((err = edb_wal_read(edbHndl, walOffset, (void*) buff, _OS_getPageSize())))
					break;
				walOffset += _OS_getPageSize();

				cumulateBlockCRC(edbHndl, (void*) buff, _OS_getPageSize());
				xPgCt++;
			}

			if (entry.type == FPM_WAL_X_COMMIT) {
				if (aheadOfDB) {
					recoveredXCt++;
					recoveredPgCt += xPgCt;
				}
				xPgCt = 0;
			} else if (entry.type == FPM_WAL_X_CANCEL) {
				xPgCt = 0;
			} //else if (entry.type == FPM_WAL_X_START) {
//				xPgCt = 0;
//			}

		}
	} while (walOffset < walEndLoc);

	if (edbHndl->x.active != NULL) {
#if EDB_ENABLE_DIAG_WAL
		EDB_PRINT_I("WAL purging truncated X %d, pgCt %d", edbHndl->x.active->xId, xPgCt);
#endif
		edb_trns_cancelMetaData(edbHndl->x.active); //Cancel any un-committed x
	}

	if (!aheadOfDB) {
		//DB is current with this WAL
		edb_walIdx_purge(edbHndl);
		err = EDB_OKAY;
	} else {
#if EDB_ENABLE_DIAG_WAL
		EDB_PRINT_I("WAL recovered, xCt %d, pgCt %d", recoveredXCt, recoveredPgCt);
#endif
	}

	if (err == (EDB_result_t) EDB_PM_IO_ERROR) {
	} else {
		if (err == EDB_WAL_FRMT_VIOLATION) {
			_ASSERT_DEBUG(0);
			err = EDB_WAL_DIRTY;
		}
	}

	if (maxPgIdx >= edbHndl->fpm.s.usedPageCount)
		edbHndl->fpm.s.usedPageCount = maxPgIdx + 1;

	_ASSERT(edbHndl->x.active == NULL);

	edbHndl->xIdNext = 1;//edbHndl->x.prev.xId + 1;
	edbHndl->wal.salt = header.salt;

	edbHndl->wal.activeWritePgIdx = walOffset / _OS_getPageSize();
	edbHndl->wal.activeWritePgPos = walOffset % _OS_getPageSize();
	edbHndl->wal.activeWritePos = walOffset;

	return err;
}

#endif
#endif

