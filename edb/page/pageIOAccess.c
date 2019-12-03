/*
 * pageAccess.c
 *
 */

#include <edb/config.h>
#if INCLUDE_EDB

#include "stdlib.h"
#include "string.h"

#include "lib/crc.h"
#include "lib/assert.h"
#include <edb/pageMgr/pm_prv.h>
#include "pageIOAccess_prv.h"
#include "../pageHeaders/pageHeader.h"
#include "rawPageIOAccess_prv.h"


EDB_result_t fpm_readPage(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx, void* buf) {
	edb_pm_pageHeader_t* ph;
	int err;
	void* crcRegion;
	uint32_t crc;

	if ((err = fpm_readRawPage(pgMgrHndl, pgIdx, buf, 1)))
		return err;

	ph = (edb_pm_pageHeader_t*) buf;
	if (ph->type == EDB_PM_PT_UNKNOWN)
		return EDB_OKAY_UNINITIALIZED;

	_ASSERT_DEBUG(pgIdx == 0 || ph->pageNumber == pgIdx);
	if (ph->pageNumber != pgIdx)
		return EDB_PM_PAGE_CORRUPTED;

	crcRegion = (void*) ((uint8_t*) buf + sizeof(ph->crc));
	crc = __CRC32(crcRegion, _OS_getPageSize() - sizeof(ph->crc), 0xFFFFFFFF);
	if (ph->crc != crc) {
		EDB_PRINT_E("EDB PM Corrupted Page CRC");
		return edb_handleError(pgMgrHndl, EDB_PM_PAGE_CORRUPTED_CRC);
	}

	return EDB_OKAY;
}

EDB_result_t fpm_writePage(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx, void* buf) {
	edb_pm_pageHeader_t* ph;
	int err;
	void* crcRegion;
	uint32_t crc;

#if EDB_QA
	if (pgIdx == 0 && pgMgrHndl->qa_BlockHeaderUpdate)
		return EDB_OKAY;
#endif

	ph = (edb_pm_pageHeader_t*) buf;
	_ASSERT(!(ph->type == EDB_PM_PT_EMPTY_ALLOCATED));
	ph->pageNumber = pgIdx;

	crcRegion = (void*) ((uint8_t*) buf + sizeof(ph->crc));
	crc = __CRC32(crcRegion, _OS_getPageSize() - sizeof(ph->crc), 0xFFFFFFFF);
	ph->crc = crc;

	if ((err = fpm_writeRawPage(pgMgrHndl, pgIdx, buf, 1)))
		return err;

	pgMgrHndl->fpm.stats.pageWriteCt++;

	return EDB_OKAY;
}

static EDB_result_t prepPageWrite(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx, void* buf) {
	edb_pm_pageHeader_t* ph;
	void* crcRegion;
	uint32_t crc;

#if EDB_QA
	if (pgIdx == 0 && pgMgrHndl->qa_BlockHeaderUpdate)
		return EDB_OKAY;
#endif

	ph = (edb_pm_pageHeader_t*) buf;
	_ASSERT(ph->type != EDB_PM_PT_EMPTY_ALLOCATED);
	ph->pageNumber = pgIdx;

	crcRegion = (void*) ((uint8_t*) buf + sizeof(ph->crc));
	crc = __CRC32(crcRegion, _OS_getPageSize() - sizeof(ph->crc), 0xFFFFFFFF);
	ph->crc = crc;

	pgMgrHndl->fpm.stats.pageWriteCt++;

	//if(ph->crc == crc)
	//	return EDB_OKAY;


	return EDB_OKAY;
}

EDB_result_t fpm_writePages(EDB_Hndl_t pgMgrHndl, uint32_t num, edb_pm_pgBufHndl_t* bufs) {
	EDB_result_t err;
	pgIdx_t pgIdx;
	void* mergedBuffer = NULL;

	if (num) {
		mergedBuffer = _OS_MAlloc(num * _OS_getPageSize());
	}

	pgIdx = bufs[0]->pgIdx;

	for (uint32_t i = 0; i < num; i++) {
		if (mergedBuffer) {
			if ((err = prepPageWrite(pgMgrHndl, pgIdx, bufs[i]->p))) {
				_OS_Free(mergedBuffer);
				return err;
			}
			void* dest = ((uint8_t*) mergedBuffer + i * _OS_getPageSize());
			memcpy(dest, bufs[i]->p, _OS_getPageSize());
		} else {
			fpm_writePage(pgMgrHndl, pgIdx, bufs[i]->p);
		}
		pgIdx++;
	}

	if (mergedBuffer) {
		err = fpm_writeRawPage(pgMgrHndl, bufs[0]->pgIdx, mergedBuffer, num);

		_OS_Free(mergedBuffer);
	}

	return err;
}

#endif
