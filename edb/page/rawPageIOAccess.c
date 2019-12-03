/*
 * rawPageIOAccess.c
 *
 */

#include <edb/config.h>
#if INCLUDE_EDB

#include "stdlib.h"
#include "string.h"
#include "stdio.h"

#include "lib/assert.h"
#include "../error.h"
#include "pageIOAccess_prv.h"

#include "rawPageIOAccess_prv.h"


EDB_result_t fpm_readRawPage(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx, void* buf, uint32_t pgCt) {
		int err;
	int32_t fOffset;
	int32_t ioOpLen;

	fOffset = (pgIdx + pgMgrHndl->fpm.firstPageOffset);
	if (!pgMgrHndl->isIOBlockMode) {
		fOffset *= _OS_getPageSize();
		ioOpLen = _OS_getPageSize() * pgCt;
	} else {
		ioOpLen = pgCt;
	}

	_ASSERT_DEBUG((pgIdx >= pgMgrHndl->fpm.firstPageOffset && pgMgrHndl->fpm.s.maxPageCount == 0) || (pgIdx < (pgMgrHndl->fpm.firstPageOffset + pgMgrHndl->fpm.s.maxPageCount)));

	if ((err = fseek(pgMgrHndl->fpm.fd_ptr, fOffset, SEEK_SET)))
		return edb_handleIOError(pgMgrHndl, err);

	err = fread(buf, 1, ioOpLen, pgMgrHndl->fpm.fd_ptr);
	if (ioOpLen != err)
		return edb_handleIOError(pgMgrHndl, ferror(pgMgrHndl->fpm.fd_ptr));


	return EDB_OKAY;
}

EDB_result_t fpm_writeRawPage(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx, void* buf, uint32_t pgCt) {
	int err;
	int32_t fOffset;
	int32_t ioOpLen;

	fOffset = (pgIdx + pgMgrHndl->fpm.firstPageOffset);
	if (!pgMgrHndl->isIOBlockMode) {
		fOffset *= _OS_getPageSize();
		ioOpLen = _OS_getPageSize() * pgCt;
	} else {
		ioOpLen = 1 * pgCt;
	}

	_ASSERT_DEBUG((pgIdx >= pgMgrHndl->fpm.firstPageOffset && pgMgrHndl->fpm.s.maxPageCount == 0) || (pgIdx < (pgMgrHndl->fpm.firstPageOffset + pgMgrHndl->fpm.s.maxPageCount)));

	if ((err = fseek(pgMgrHndl->fpm.fd_ptr, fOffset, SEEK_SET)))
		return edb_handleIOError(pgMgrHndl, err);

	if (ioOpLen != fwrite(buf, 1, ioOpLen, pgMgrHndl->fpm.fd_ptr))
		return edb_handleIOError(pgMgrHndl, ferror(pgMgrHndl->fpm.fd_ptr));

	return EDB_OKAY;
}

EDB_result_t FPM_QA_writeRawPage(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx, void* buf) {
	int err;
	int32_t fOffset;

	fOffset = (pgIdx + pgMgrHndl->fpm.firstPageOffset);
	if (!pgMgrHndl->isIOBlockMode)
		fOffset *= _OS_getPageSize();

	_ASSERT_DEBUG((pgIdx >= pgMgrHndl->fpm.firstPageOffset && pgMgrHndl->fpm.s.maxPageCount == 0) || (pgIdx < (pgMgrHndl->fpm.firstPageOffset + pgMgrHndl->fpm.s.maxPageCount)));

	if ((err = fseek(pgMgrHndl->fpm.fd_ptr, fOffset, SEEK_SET)))
		return edb_handleIOError(pgMgrHndl, err);

	if (_OS_getPageSize() != fwrite(buf,1, _OS_getPageSize(), pgMgrHndl->fpm.fd_ptr))
		return edb_handleIOError(pgMgrHndl, ferror(pgMgrHndl->fpm.fd_ptr));

	return EDB_OKAY;
}


EDB_result_t fpm_writeRawPages(EDB_Hndl_t pgMgrHndl, uint32_t num, pgIdx_t startPgIdx, void** bufs) {
	EDB_result_t err = EDB_OKAY;
	pgIdx_t pgIdx;
	void* mergedBuffer = NULL;
	int32_t ioOpLen;

	if (num) {
		mergedBuffer = _OS_MAlloc(num * _OS_getPageSize());
	}

	pgIdx = startPgIdx;

	_ASSERT_DEBUG((pgIdx >= pgMgrHndl->fpm.firstPageOffset && pgMgrHndl->fpm.s.maxPageCount == 0) || ((pgIdx + num) < (pgMgrHndl->fpm.firstPageOffset + pgMgrHndl->fpm.s.maxPageCount)));

	for (uint32_t i = 0; i < num; i++) {
		if (mergedBuffer) {
			void* dest = ((uint8_t*) mergedBuffer + i * _OS_getPageSize());
			memcpy(dest, (const void*)bufs[i], (size_t)_OS_getPageSize());
		} else {
			fpm_writePage(pgMgrHndl, pgIdx, bufs[i]);
		}
		pgIdx++;
	}

	if (mergedBuffer) {
		uint32_t startFOffset = (startPgIdx + pgMgrHndl->fpm.firstPageOffset);
		if (!pgMgrHndl->isIOBlockMode) {
			startFOffset *= _OS_getPageSize();
			ioOpLen = _OS_getPageSize() * num;
		} else {
			ioOpLen = num;
		}

		if ((err = fseek(pgMgrHndl->fpm.fd_ptr, startFOffset, SEEK_SET))){
			err = edb_handleIOError(pgMgrHndl, err);
			goto error;
		}

		if (ioOpLen != fwrite(mergedBuffer, 1, ioOpLen,pgMgrHndl->fpm.fd_ptr)){
			err = edb_handleIOError(pgMgrHndl, ferror(pgMgrHndl->fpm.fd_ptr));
				goto error;
		}


	}

	error:
	if(mergedBuffer)
		_OS_Free(mergedBuffer);

	return err;
}


#endif
