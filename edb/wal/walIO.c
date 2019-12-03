/*
 * walIO.c
 *
 */

#include <edb/config.h>
#if INCLUDE_EDB
#if EDB_ENABLE_X

#include "stdlib.h"
#include "string.h"

#include <edb/pageMgr/pm_prv.h>
#include "walIndex.h"
#include "wal_prv.h"
#include "../transaction/transaction_prv.h"

static EDB_result_t edb_wal_getPageFromWal(EDB_Hndl_t edbHndl, pgIdx_t pgIdx, uint32_t walOffset, void* buf);
static EDB_result_t prepPageAppend(EDB_Hndl_t edbHndl, EDB_xHndl_t xHndl, FPM_WAL_entry_t* entry, pgIdx_t pgIdx,
		void* buf, uint32_t* wallOffset);

EDB_result_t edb_wal_readPage_fromFPM(EDB_Hndl_t edbHndl, pgIdx_t pgIdx, void* buf) {
	EDB_result_t err;
	uint32_t walOffset;


#if !IS_PROD_MODE
	if (edbHndl->isWalRecovering || edbHndl->isWalSyncing){
		_ASSERT_DEBUG(edbHndl->x.active == NULL);
		//_ASSERT_DEBUG(edbHndl->x.active->state != EDB_X_STATE_ACTIVE);
	}else{
		_ASSERT_DEBUG(edbHndl->x.active != NULL);
		_ASSERT_DEBUG(edbHndl->x.active->state == EDB_X_STATE_ACTIVE);
	}
#endif

	if ((err = edb_walIdx_find(edbHndl, pgIdx, &walOffset)))
		return err;
	err = edb_wal_getPageFromWal(edbHndl, pgIdx, walOffset, buf);
	return err;
}

void edb_wal_io_resetCache(EDB_Hndl_t edbHndl) {
	edbHndl->wal.appedBufferValid = false;
	edbHndl->wal.readBufferValid = false;
}
EDB_result_t edb_wal_io_flush(EDB_Hndl_t edbHndl) {
	EDB_result_t err = EDB_OKAY;
	if (edbHndl->wal.appedBufferValid && edbHndl->wal.appedBufferDirty) {
		if ((err = (EDB_result_t) fpm_writeRawPage(edbHndl, edbHndl->wal.activeWritePgIdx, edbHndl->wal.appendBuffer, 1)))
			return err;
		edbHndl->wal.appedBufferDirty = false;
	}

	return err;
}
EDB_result_t edb_wal_io_write(EDB_Hndl_t edbHndl, void* buf, uint32_t len) {
	EDB_result_t err;

	//void* activePg;
	uint32_t activeWritePgIdx;
	uint32_t activeWritePgPos;

	uint32_t remainingSpaceInPage;

	uint8_t* src;
	uint8_t* dst;
	uint32_t xCnt;
	uint32_t remaining;

	//activePg = edbHndl->wal.activeAppendPage;
	activeWritePgPos = edbHndl->wal.activeWritePgPos;
	activeWritePgIdx = edbHndl->wal.activeWritePgIdx;

	remainingSpaceInPage = _OS_getPageSize() - activeWritePgPos;
	src = (uint8_t*) buf;
	remaining = len;

	while (remaining) {
		xCnt = remaining;

		if (!edbHndl->wal.appedBufferValid) {
			_ASSERT_DEBUG(activeWritePgPos == 0);
			if (xCnt > _OS_getPageSize()) {
				uint32_t pgCt;
				pgCt = xCnt / _OS_getPageSize();
				xCnt = pgCt * _OS_getPageSize();

				if ((err = (EDB_result_t) fpm_writeRawPage(edbHndl, activeWritePgIdx, src, pgCt)))
					return err;

				src += xCnt;
				activeWritePgIdx += pgCt;
				remaining -= xCnt;
				continue;

			} else {
				if (activeWritePgPos == 0) {
					//not really even needed
					//memset(edbHndl->wal.appendBuffer, 0, _OS_getPageSize());
				} else {
					if ((err = (EDB_result_t) fpm_readRawPage(edbHndl, activeWritePgIdx, edbHndl->wal.appendBuffer, 1)))
						return err;
				}

				edbHndl->wal.appedBufferValid = true;
				remainingSpaceInPage = _OS_getPageSize();
				activeWritePgPos = 0;
			}
		}

		if (xCnt > remainingSpaceInPage)
			xCnt = remainingSpaceInPage;

		dst = ((uint8_t*) edbHndl->wal.appendBuffer) + activeWritePgPos;

		memcpy(dst, src, xCnt);

		remaining -= xCnt;
		activeWritePgPos += xCnt;
		edbHndl->wal.appedBufferDirty = true;

		src += xCnt;

		if (activeWritePgPos == _OS_getPageSize()) {
			if ((err = (EDB_result_t) fpm_writeRawPage(edbHndl, activeWritePgIdx, edbHndl->wal.appendBuffer, 1)))
				return err;

			activeWritePgIdx++;
			activeWritePgPos = 0;
			edbHndl->wal.appedBufferDirty = false;
			edbHndl->wal.appedBufferValid = false;
		} else {
			_ASSERT_DEBUG(remaining == 0);
		}
	}

	//Sync with read buffer
	if (edbHndl->wal.appedBufferDirty && edbHndl->wal.readBufferValid
			&& (edbHndl->wal.activeWritePgIdx == edbHndl->wal.readBufferPgIdx + 1
					|| edbHndl->wal.activeWritePgIdx == edbHndl->wal.readBufferPgIdx)) {
		edbHndl->wal.readBufferValid = false;
	}
	//edbHndl->wal.activeAppendPage = activePg;
	edbHndl->wal.activeWritePgIdx = activeWritePgIdx;
	edbHndl->wal.activeWritePgPos = activeWritePgPos;
	edbHndl->wal.activeWritePos += len;

	return err;
}

EDB_result_t edb_wal_writePage_fromFPM(EDB_Hndl_t edbHndl, edb_pm_pgBufHndl_t pgBuf) {		//pgIdx_t pgIdx, void* buf) {
	EDB_result_t err;
	FPM_WAL_entry_t entry;
	uint32_t futureWallOffset;

	_ASSERT_DEBUG(edbHndl->x.active->state == EDB_X_STATE_ACTIVE);
	_ASSERT_DEBUG(edbHndl->x.active->isWriter);
	//if (!edbHndl->x.active->writer) {
//		edb_trns_makeWriter(edbHndl->x.active);
	//	//err = edb_wal_wrtXStart(edbHndl, edbHndl->x.active);
	//}

	_ASSERT_DEBUG(((edb_pm_pageHeader_t* )pgBuf->p)->pageNumber == pgBuf->pgIdx);

	futureWallOffset = edbHndl->wal.activeWritePos;
	if ((err = prepPageAppend(edbHndl, edbHndl->x.active, &entry, pgBuf->pgIdx, pgBuf->p, &futureWallOffset))) {
		if (err == EDB_WAL_OVERFLOW) {
			edb_trns_failed(edbHndl->x.active);
		}
		return err;
	}

	err = edb_wal_io_write(edbHndl, (void*) &entry, sizeof(FPM_WAL_entry_t));
	edb_trns_addPg(pgBuf->tranOwner, pgBuf->pgIdx, edbHndl->wal.activeWritePos);
	//err = edb_walIdx_addPageToX(edbHndl, pgBuf->pgIdx, edbHndl->wal.activeWritePos);
	err = edb_wal_io_write(edbHndl, (void*) pgBuf->p, _OS_getPageSize());

	return err;
}

EDB_result_t edb_wal_getPageFromWal(EDB_Hndl_t edbHndl, pgIdx_t pgPgIdx, uint32_t walOffset, void* buf) {
	EDB_result_t err;

#if EDB_ENABLE_DIAG_WAL
	uint32_t pageOffset;
	FPM_WAL_entry_t entry;
	uint8_t tmp[1024];
	memcpy(tmp, edbHndl->wal.readBuffer, 1024);
	pageOffset = walOffset - sizeof(FPM_WAL_entry_t);
	err = edb_wal_read(edbHndl, pageOffset, (void*) &entry, sizeof(FPM_WAL_entry_t));
	_ASSERT_DEBUG(entry.type == FPM_WAL_PAGE && entry.page.pgIdx == pgPgIdx);
	_ASSERT_DEBUG(entry.salt == edbHndl->wal.salt);
#endif

	err = edb_wal_read(edbHndl, walOffset, buf, _OS_getPageSize());

#if EDB_ENABLE_DIAG_WAL
	_ASSERT_DEBUG(entry.type == FPM_WAL_PAGE && entry.page.pgIdx == pgPgIdx);
	_ASSERT_DEBUG(entry.salt == edbHndl->wal.salt);
#endif

	return err;
}

EDB_result_t edb_wal_read(EDB_Hndl_t edbHndl, uint32_t walOffset, void* buf, uint32_t len) {
	EDB_result_t err;

	pgIdx_t pgIdx;
	pgIdx_t pgPos;

	pgIdx = edbHndl->wal.startPgIdx + (walOffset / _OS_getPageSize());
	pgPos = walOffset % _OS_getPageSize();

	if (edbHndl->wal.readBufferValid) {
		if (pgIdx == edbHndl->wal.readBufferPgIdx) {
			memcpy(buf, ((uint8_t*) edbHndl->wal.readBuffer) + pgPos, len);
			return EDB_OKAY;
		} else if (pgIdx == (edbHndl->wal.readBufferPgIdx + 1) && (len + pgPos) < _OS_getPageSize()) {
			memcpy(buf, ((uint8_t*) edbHndl->wal.readBuffer) + _OS_getPageSize() + pgPos, len);
			return EDB_OKAY;
		}
	}

	if ((err = fpm_readRawPage(edbHndl, pgIdx, edbHndl->wal.readBuffer, 2))) {
		edbHndl->wal.readBufferValid = false;
		return err;
	}

	//Sync with appendBuffer
	if (edbHndl->wal.appedBufferValid && edbHndl->wal.appedBufferDirty) {
		if (pgIdx == edbHndl->wal.activeWritePgIdx) {
			memcpy(edbHndl->wal.readBuffer, edbHndl->wal.appendBuffer, _OS_getPageSize());
		} else if (pgIdx + 1 == edbHndl->wal.activeWritePgIdx) {
			memcpy(((uint8_t*) edbHndl->wal.readBuffer) + _OS_getPageSize(), edbHndl->wal.appendBuffer, _OS_getPageSize());
		}
	}

	edbHndl->wal.readBufferValid = true;
	edbHndl->wal.readBufferPgIdx = pgIdx;

	memcpy(buf, ((uint8_t*) edbHndl->wal.readBuffer) + pgPos, len);

	return err;
}

EDB_result_t prepPageAppend(EDB_Hndl_t edbHndl, EDB_xHndl_t xHndl, FPM_WAL_entry_t* entry, pgIdx_t pgIdx, void* buf,
		uint32_t* inOut_futureWallOffset) {

	_ASSERT_DEBUG(edbHndl->wal.size - edbHndl->wal.activeWritePgPos > 2 * _OS_getPageSize());

#if EDB_ENABLE_DIAG_WAL
	EDB_PRINT_I("WAL Append pgIdx %d @ %d", pgIdx, *inOut_futureWallOffset);
#endif

	*inOut_futureWallOffset += (_OS_getPageSize() + sizeof(FPM_WAL_entry_t));

	if (*inOut_futureWallOffset > edbHndl->wal.size)
		return EDB_WAL_OVERFLOW;

	entry->type = FPM_WAL_PAGE;
	entry->page.pgIdx = pgIdx;
	entry->page.xId = xHndl->xId;
	entry->salt = edbHndl->wal.salt;

	entry->crc = __CRC32((const char*) entry, offsetof(FPM_WAL_entry_t, crc), edbHndl->wal.cumulativeCRC);

	edbHndl->wal.cumulativeCRC = __CRC32((const char*) buf, _OS_getPageSize(), entry->crc);

	return EDB_OKAY;
}

EDB_result_t edb_wal_writePages_fromFPM(EDB_Hndl_t edbHndl, uint32_t num, edb_pm_pgBufHndl_t* bufs) {
	EDB_result_t err;
	pgIdx_t pgIdx;
	uint32_t* walOffsets;
	FPM_WAL_entry_t entry;
	uint32_t pgWALOffset;
	uint32_t futureWallOffset;
	void* buff;
	uint32_t startPgIdx;
	void* mergedBuffer = NULL;
	uint32_t mergedBufferLen;

	_ASSERT_DEBUG(edbHndl->x.active != NULL);
	_ASSERT_DEBUG(edbHndl->x.active->isWriter);

	startPgIdx = bufs[0]->pgIdx;

//	if (!edbHndl->x.active->writer) {
//		edb_trns_makeWriter(edbHndl->x.active);
//		//err = edb_wal_wrtXStart(edbHndl, edbHndl->x.active);
//	}

	if (num) {

		mergedBufferLen = num * (_OS_getPageSize() + sizeof(FPM_WAL_entry_t));
		buff = _OS_MAlloc(num * sizeof(uint32_t) + mergedBufferLen);

		walOffsets = buff;
		mergedBuffer = (void*) ((uint8_t*) buff + (num * sizeof(uint32_t)));
	}

	if (!buff) {
		pgIdx = startPgIdx;
		for (uint32_t i = 0; i < num; i++) {
			err = edb_wal_writePage_fromFPM(edbHndl, bufs[i]);
			pgIdx++;
		}
		return EDB_OKAY;
	}

	pgIdx = startPgIdx;
	void* dest = ((uint8_t*) mergedBuffer);

	pgWALOffset = edbHndl->wal.activeWritePos;
	futureWallOffset = edbHndl->wal.activeWritePos;

	for (uint32_t i = 0; i < num; i++) {
		_ASSERT_DEBUG(((edb_pm_pageHeader_t* )bufs[i]->p)->pageNumber == pgIdx);
		if ((err = prepPageAppend(edbHndl, edbHndl->x.active, &entry, pgIdx, bufs[i]->p, &futureWallOffset))) {
			if (err == EDB_WAL_OVERFLOW) {
				edb_trns_failed(edbHndl->x.active);
			}
			_OS_Free(buff);
			return err;
		}
		pgWALOffset += sizeof(FPM_WAL_entry_t);
		walOffsets[i] = pgWALOffset;
		pgWALOffset += _OS_getPageSize();

		memcpy(dest, &entry, sizeof(FPM_WAL_entry_t));
		dest = (uint8_t*) dest + sizeof(FPM_WAL_entry_t);

		memcpy(dest, bufs[i]->p, _OS_getPageSize());
		dest = (uint8_t*) dest + _OS_getPageSize();

		pgIdx++;
	}

	err = edb_wal_io_write(edbHndl, (void*) mergedBuffer, mergedBufferLen);

	//err = FPM_RR_write(edbHndl, (void*) mergedBuffer, mergedBufferLen);
	if (!err) {
		pgIdx = startPgIdx;
		for (uint32_t i = 0; i < num; i++) {
			edb_trns_addPg(bufs[i]->tranOwner, pgIdx, walOffsets[i]);
			pgIdx++;
		}
	}

	_OS_Free(buff);

	return EDB_OKAY;
}

#endif
#endif
