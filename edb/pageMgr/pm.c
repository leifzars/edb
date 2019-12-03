/*
 * pageManager.c
 *
 */

#include <edb/config.h>
#if INCLUDE_EDB

#include <edb/config.h>

#include <string.h>
#include "time.h"

#include <edb/pageMgr/pm_prv.h>
#include "../pageHeaders/freePageHeader.h"



#define OSL_FILE_PATH			"\\logs"
#define OSL_FILE_NAME_PREFIX	"Sys"
#define OSL_FILE_NAME_POST		"os"


static EDB_result_t writePMHeader(EDB_Hndl_t pgMgrHndl, bool flush);
static EDB_result_t loadPMHeader(EDB_Hndl_t pgMgrHndl, bool resetIfCorrupted,
bool resetRegardless);
static void initNewPMFileHeader(EDB_Hndl_t edb, FPM_fileHeader_t* fpmHeader);
static bool fpm_spaceAvailable(EDB_Hndl_t pgMgrHndl);
static bool setMaxPageCount(EDB_Hndl_t pgMgrHndl, uint32_t count);

void FPM_init() {


}

//EDB_result_t fpm_markDirty(FPM_Hndl_t pgMgrHndl) {
//	pgMgrHndl->isHeaderDirty = true;
////	if (!pgMgrHndl->fpm.s.isDirty) {
////		pgMgrHndl->fpm.s.isDirty = true;
////		return writePMHeader(pgMgrHndl, true);
////	}
//	return EDB_OKAY;
//}
//
//static EDB_result_t getTempPMHeader(pgMgrHndl_t pgMgrHndl) {
//	EDB_result_t err;
//	//FPM_fileHeader_t* fh;
//	void* pgBuf;
//	err = fpm_getPage(pgMgrHndl, 0, true, (void**) &pgBuf);
//
//	_ASSERT_DEBUG(pgMgrHndl->tmpFileHeaderBuffer == NULL);
//	pgMgrHndl->tmpFileHeaderBuffer = (FPM_fileHeader_t*) pgBuf;
//	return EDB_OKAY;
//}
//
//static EDB_result_t returnTempPMHeader(pgMgrHndl_t pgMgrHndl, bool dirtyAndFlush) {
//	EDB_result_t err;
//	//FPM_fileHeader_t* fh;
//	void* pgBuf;
//	_ASSERT_DEBUG(pgMgrHndl->tmpFileHeaderBuffer != NULL);
//
//	err = fpm_returnPage(pgMgrHndl, pgMgrHndl->tmpFileHeaderBuffer, dirtyAndFlush);
//
//	if (dirtyAndFlush)
//		fpm_flushPage(pgMgrHndl, 0);
//
//	pgMgrHndl->tmpFileHeaderBuffer = NULL;
//	return EDB_OKAY;
//}

EDB_result_t writePMHeader(EDB_Hndl_t pgMgrHndl, bool flush) {
	EDB_result_t err;
	//FPM_fileHeader_t* fh;
	void* pgBuf;
	if((err = FPM_getPage(pgMgrHndl, FPM_HEADER_PG_IDX, false, (void**) &pgBuf)))
		return err;

	memcpy(pgBuf, (void*) &pgMgrHndl->fpm.fileHeader, sizeof(FPM_fileHeader_t));

	fpm_returnPage(pgMgrHndl, pgBuf, FPM_PGBUF_FLUSH);

	pgMgrHndl->fpm.isHeaderDirty = false;
	//if (flush)
	//fpm_flushPage(pgMgrHndl, 0);
	return EDB_OKAY;
}

EDB_result_t edb_pm_reLoadPMHeader(EDB_Hndl_t pgMgrHndl) {
	return loadPMHeader(pgMgrHndl, false, false);
}

EDB_result_t loadPMHeader(EDB_Hndl_t pgMgrHndl, bool resetIfCorrupted,
bool resetRegardless) {
	EDB_result_t err;
	//FPM_fileHeader_t* fh;
	void* pgBuf;
//
//	if (resetRegardless) {
//		initNewPMFileHeader(pgMgrHndl, &pgMgrHndl->fpm.s);
//		pgMgrHndl->isNew = true;
//		if ((err = writePMHeader(pgMgrHndl, true)))
//			return err;
//	}

	err = FPM_getPage(pgMgrHndl, FPM_HEADER_PG_IDX, true, (void**) &pgBuf);
	pgMgrHndl->oppendWithCorruptHeader = err == EDB_PM_PAGE_CORRUPTED_CRC || err == EDB_PM_PAGE_CORRUPTED;

	if (resetRegardless || (resetIfCorrupted && pgMgrHndl->oppendWithCorruptHeader)) {
		fpm_returnPage(pgMgrHndl, pgBuf, FPM_PGBUF_NONE);
		initNewPMFileHeader(pgMgrHndl, &pgMgrHndl->fpm.s);
		//err = writePMHeader(pgMgrHndl, true);
		if ((err = writePMHeader(pgMgrHndl, true)))
			return err;
		pgMgrHndl->isNew = true;
		err = EDB_OKAY;
	} else if (err == EDB_OKAY) {
		memcpy((void*) &pgMgrHndl->fpm.fileHeader, pgBuf, sizeof(FPM_fileHeader_t));
		fpm_returnPage(pgMgrHndl, pgBuf, FPM_PGBUF_DIRTY);
		if (pgMgrHndl->fpm.s.magicNumber != FPM_FILE_MAGIC_NUMBER) {
			return EDB_PM_ERROR_UNKNOWN_FORMAT;
		}
		pgMgrHndl->fpm.isHeaderDirty = false;
	} else {
		//some other error
		fpm_returnPage(pgMgrHndl, pgBuf, FPM_PGBUF_NONE);
	}

	return err;
}

pgIdx_t edb_getResourcePgIdx(EDB_Hndl_t pgMgrHndl, edb_pm_resource_e type) {

	for (int i = 0; i < FPM_MAX_RESOURCE_COUNT; i++) {
		if (pgMgrHndl->fpm.s.resources[i].type == type) {
			return pgMgrHndl->fpm.s.resources[i].pagIdx;
		}
	}
	return 0;
}

void edb_setResourcePgIdx(EDB_Hndl_t pgMgrHndl, edb_pm_resource_e type, pgIdx_t pgIdx) {

	for (int i = 0; i < FPM_MAX_RESOURCE_COUNT; i++) {
		if (pgMgrHndl->fpm.s.resources[i].type == type) {
			if (pgMgrHndl->fpm.s.resources[i].pagIdx != pgIdx) {
				pgMgrHndl->fpm.s.resources[i].pagIdx = pgIdx;
				edb_pm_markDirty(pgMgrHndl);
			}
			return;
		}
	}

	for (int i = 0; i < FPM_MAX_RESOURCE_COUNT; i++) {
		if (pgMgrHndl->fpm.s.resources[i].type == FPM_R_UNKNOWN) {
			pgMgrHndl->fpm.s.resources[i].type = type;
			pgMgrHndl->fpm.s.resources[i].pagIdx = pgIdx;
			edb_pm_markDirty(pgMgrHndl);
			return;
		}
	}
	_ASSERT(0);
}

void initNewPMFileHeader(EDB_Hndl_t edb, FPM_fileHeader_t* fpmHdr) {

	memset(fpmHdr, 0, sizeof(FPM_fileHeader_t));

	fpmHdr->base.type = EDB_PM_PT_HEADER;
	fpmHdr->base.pageNumber = 0;

	fpmHdr->createTime = time(NULL);
	fpmHdr->isDirty = false;
	fpmHdr->isOpen = true;
	fpmHdr->magicNumber = FPM_FILE_MAGIC_NUMBER;
	fpmHdr->version = FPM_V_1;

	fpmHdr->usedPageCount = 1;

	fpmHdr->freePageCount = 0;
	fpmHdr->nextFreePage = 0;
	fpmHdr->maxPageCount = 0;

	edb_pm_markDirty(edb);

}

pgIdx_t fOffsetToPage(uint32_t fOffset) {
	pgIdx_t pageIdx;

	pageIdx = fOffset >> EDB_PAGE_BIT_POSS;

	if (fOffset & EDB_PAGE_MASK)
		pageIdx++;
	return pageIdx;
}

pageType_e edb_pm_getPageType(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx) {
	EDB_result_t err;
	pageType_e pgType;
	edb_pm_pageHeader_t* ph;

	if ((err = FPM_getPage(pgMgrHndl, pgIdx, true, (void**) &ph)))
		return err;

	pgType = ph->type;
	fpm_returnPage(pgMgrHndl, (void*) ph, FPM_PGBUF_NONE);
	return pgType;
}

EDB_result_t edb_getFreePage(EDB_Hndl_t pgMgrHndl, uint32_t* pageIdx) {
	int err;
	freePageHeader_t* fph;

	if (pgMgrHndl->fpm.s.nextFreePage) {
		_ASSERT_DEBUG(pgMgrHndl->fpm.s.freePageCount);
		err = FPM_getPage(pgMgrHndl, pgMgrHndl->fpm.s.nextFreePage, true, (void **)&fph);
		if (err)
			return err;

		_ASSERT_DEBUG(fph->base.type == EDB_PM_PT_FREE);

		*pageIdx = pgMgrHndl->fpm.s.nextFreePage;
		pgMgrHndl->fpm.s.nextFreePage = fph->nextPgIdx;
		pgMgrHndl->fpm.s.freePageCount--;
		edb_pm_markDirty(pgMgrHndl);

		pgMgrHndl->fpm.stats.pageAllocCt++;

		fph->base.type = EDB_PM_PT_EMPTY_ALLOCATED; //pgType;//STORE_V_ALLOCATED;
		fpm_returnPage(pgMgrHndl, (void*) fph, FPM_PGBUF_DIRTY_ALLOCATED);

		return EDB_OKAY;

	} else {
		_ASSERT_DEBUG(pgMgrHndl->fpm.s.freePageCount == 0);
		if (!fpm_spaceAvailable(pgMgrHndl))
			return EDB_PM_IO_ERROR_FULL;

		*pageIdx = pgMgrHndl->fpm.s.usedPageCount++;
		edb_pm_markDirty(pgMgrHndl);

		return EDB_OKAY;
	}
}

EDB_result_t edb_freePage(EDB_Hndl_t pgMgrHndl, uint32_t pageIdx) {
	int err;
	freePageHeader_t* fph;
	_ASSERT_DEBUG(pageIdx < pgMgrHndl->fpm.s.usedPageCount);

	if (pageIdx == 0)
		return EDB_PM_ERROR_ILLEGAL_OPERATION;

	if ((err = FPM_getPage(pgMgrHndl, pageIdx, false, (void **)&fph)))
		return err;

	fph->base.crc = 0;
	fph->nextPgIdx = pgMgrHndl->fpm.s.nextFreePage;
	fph->base.pageNumber = pageIdx;
	fph->base.type = EDB_PM_PT_FREE;
	fpm_returnPage(pgMgrHndl, (void*) fph, FPM_PGBUF_DIRTY);

	pgMgrHndl->fpm.s.nextFreePage = pageIdx;
	pgMgrHndl->fpm.s.freePageCount++;
	edb_pm_markDirty(pgMgrHndl);

	pgMgrHndl->fpm.stats.pageFreeCt++;
	return EDB_OKAY;
}

void edb_pm_setMaxPageCount(EDB_Hndl_t pgMgrHndl, uint32_t count) {
	if (setMaxPageCount(pgMgrHndl, count)) {
		writePMHeader(pgMgrHndl, true);
	}
}

bool setMaxPageCount(EDB_Hndl_t pgMgrHndl, uint32_t count) {
	if (pgMgrHndl->fpm.s.maxPageCount == 0 || pgMgrHndl->fpm.s.maxPageCount < count) {
		pgMgrHndl->fpm.s.maxPageCount = count;
		edb_pm_markDirty(pgMgrHndl);
		return true;
	}
	return false;
}

uint32_t edb_pm_getMaxPageCount(EDB_Hndl_t pgMgrHndl) {
	return pgMgrHndl->fpm.s.maxPageCount;
}

uint32_t edb_pm_availablePages(EDB_Hndl_t pgMgrHndl) {
	uint32_t ct;
	if (pgMgrHndl->fpm.s.maxPageCount == 0)
		return 0xFFFFFFFF;
	ct = pgMgrHndl->fpm.s.maxPageCount - pgMgrHndl->fpm.s.usedPageCount;
	ct += pgMgrHndl->fpm.s.freePageCount;

	return ct;
}

uint32_t edb_pm_usedPages(EDB_Hndl_t pgMgrHndl) {
	uint32_t ct;

	ct = pgMgrHndl->fpm.s.usedPageCount;
	ct -= pgMgrHndl->fpm.s.freePageCount;

	return ct;
}

uint32_t edb_pm_consumedPages(EDB_Hndl_t pgMgrHndl) {
	return pgMgrHndl->fpm.s.usedPageCount;;
}

void FPM_simulateOutOfSyncFileHeader(EDB_Hndl_t pgMgrHndl) {
	pgMgrHndl->fpm.s.freePageCount = 0;
	pgMgrHndl->fpm.s.nextFreePage = 0;
}

bool osl_store_recover(EDB_Hndl_t pgMgrHndl) {
	fclose(pgMgrHndl->fpm.fd_ptr);
	remove(pgMgrHndl->fpm.fileName);
	pgMgrHndl->fpm.fd_ptr = NULL;
	return true;
}

void FPM_printStatus(EDB_Hndl_t fpm) {

	char tmp[64];
	EDB_PRINT_I(" ");
	EDB_PRINT_I(" Name: %s", fpm->fpm.fileName);
	if (fpm->isIOBlockMode) {
		EDB_PRINT_I(" Raw Access, offset %d", fpm->fpm.firstPageOffset);
	} else {
		EDB_PRINT_I(" File Access, %s", fpm->fpm.fileName);
	}

	//ctime(fpm->fpm.s.createTime, tmp, 64);
	EDB_PRINT_I(" Created @ %s", tmp);

	EDB_PRINT_I(" Stats & Struct Info...");
	EDB_PRINT_I("  Pg Wrt   Ct %" PRIu64, fpm->fpm.stats.pageWriteCt);
	EDB_PRINT_I("  Pg Free  Ct %" PRIu64, fpm->fpm.stats.pageFreeCt);
	EDB_PRINT_I("  Pg Alloc Ct %" PRIu64, fpm->fpm.stats.pageAllocCt);

	EDB_PRINT_I("  Used Pg Ct %d", fpm->fpm.s.usedPageCount);
	EDB_PRINT_I("  Free Pg Ct %d", fpm->fpm.s.freePageCount);
	EDB_PRINT_I("  Pg Limit %d", fpm->fpm.s.maxPageCount);

	EDB_PRINT_I(" WAL IDX %d", fpm->fpm.s.walSyncIndx);

	//EDB_PRINT_I(100, "Page Count %d", s->usedPageCount);
	//EDB_PRINT_I(100, "Free Page Count %d", pgMgrHndl->fpm.s.freePageCount);
	EDB_PRINT_I(" Resources...");
	FPM_resourceLocation_t* r;
	for (int i = 0; i < FPM_MAX_RESOURCE_COUNT; i++) {
		r = &fpm->fpm.s.resources[i];
		if (r->type == FPM_R_TABLE_DEFS) {
			EDB_PRINT_I( "  TableDef @ %d", r->pagIdx);
		} else if (r->type == FPM_R_INDEX_DEFS) {
			EDB_PRINT_I( "  IndexDef @ %d", r->pagIdx);
		} else if (r->type == FPM_R_WAL) {
			EDB_PRINT_I( "  WAL      @ %d", r->pagIdx);
		} else if (r->type != FPM_R_UNKNOWN) {
			EDB_PRINT_I( "  ????     @ %d", r->pagIdx);
		}
	}

}

void edb_pm_close(EDB_Hndl_t fpmHndl) {

	fpm_flushAll(fpmHndl);
	fflush(fpmHndl->fpm.fd_ptr);

	fpmHndl->fpm.s.isOpen = false;
	fpmHndl->fpm.s.isDirty = false;
	writePMHeader(fpmHndl, true);

	fclose(fpmHndl->fpm.fd_ptr);

	_OS_flushFat();
	fpmHndl->fpm.fd_ptr = 0;
	fpm_clearCache(fpmHndl);

	fpm_freeBuffers(fpmHndl);
}


EDB_result_t edb_pm_open(EDB_Hndl_t edb, edb_pm_openArgsPtr_t argsPtr) {
//_ASSERT_DEBUG(pgMgrHndl && pgMgrHndl->fpm.fd_ptr == NULL);
	EDB_result_t err;

	uint32_t retryCnt;

	if ((err = fpm_initBuffers(edb, EDB_PAGE_CACHE))) {
		goto openFailed;
	}


	if (argsPtr->isFile) {
		do {
			if (!argsPtr->resetAll)
				edb->fpm.fd_ptr = fopen(edb->fpm.fileName, "r+");
			else
				edb->fpm.fd_ptr = NULL;

			if (edb->fpm.fd_ptr == NULL) {
				//_task_set_error(MQX_OK);
				edb->fpm.fd_ptr = fopen(edb->fpm.fileName, "w+");
				if (edb->fpm.fd_ptr) {
					edb->isNew = true;
					EDB_PRINT_I("New page space created");
					initNewPMFileHeader(edb, &edb->fpm.s);
					edb->fpm.s.usedPageCount = edb_getPageCount(edb);
					if ((err = writePMHeader(edb, true)))
						goto openFailed;
				}
			}

			if (edb->fpm.fd_ptr == NULL) {
				err = EDB_PM_IO_OPEN_ERROR;
				goto openFailed;
			}

			edb->fpm.s.usedPageCount = edb_getPageCount(edb);

			if (edb->fpm.s.usedPageCount == 0) {
				fclose(edb->fpm.fd_ptr);
				remove(edb->fpm.fileName);
				edb->fpm.fd_ptr = NULL;
				continue;
			}
			if ((err = loadPMHeader(edb, argsPtr->resetOnCorruption, argsPtr->resetAll))) {
				goto openFailed;
			}
			if (retryCnt++ == 4) {
				err = EDB_PM_OPEN_ERROR;
				goto openFailed;
			}
		} while (edb->fpm.fd_ptr == NULL);

		edb->fpm.fileSystemAllocationSize = _OS_getPageSize();

	} else {

		edb->isIOBlockMode = true;
		edb->fpm.firstPageOffset = argsPtr->firstPageOffset;
		edb->fpm.fd_ptr = fopen(edb->fpm.fileName, NULL);
		if (edb->fpm.fd_ptr == NULL) {
			//_task_set_error(MQX_OK);
			err = EDB_PM_IO_OPEN_ERROR;
			goto openFailed;
		}

		edb->fpm.s.usedPageCount = 1; //Assume we previously used at least 1 page
		if ((err = loadPMHeader(edb, argsPtr->resetOnCorruption, argsPtr->resetAll))) {
			goto openFailed;
		} else {
			//valid Header
		}

	}

	setMaxPageCount(edb, argsPtr->maxPageCount);

	EDB_PRINT_I( "Opened name: %s", edb->fpm.fileName);

	edb->isInBadSate = false;
	if (edb->fpm.s.isDirty) {
		EDB_PRINT_W("EDB is Dirty");
		edb->needsRecovery = true;
	} else {
		edb->needsRecovery = false;
	}

	edb->fpm.s.isOpen = true;
	if ((err = writePMHeader(edb, true))) {
		goto openFailed;
	}

	edb->isReadOnly = false;

//	if ((err = BPT_open_UInt32(&fpmHndl->vIndex, fpmHndl,
//			&fpmHndl->fpm.s.indexRootPgIdxs, false))) {
//		goto openFailed;
//	}

	openFailed:

	if (err) {
		if (edb->fpm.fd_ptr)
			fclose(edb->fpm.fd_ptr);
		edb_handleError(edb, err);
	}
	return err;
}

void edb_pm_flush(EDB_Hndl_t pgMgrHndl) {
	//EDB_PRINT_I("FLUSH Cache");
	fpm_flushAll(pgMgrHndl);

	/*must flush all before writing Header
	 * Header must represent actual state, and never be ahead of DB
	 */
	pgMgrHndl->fpm.s.isDirty = false;
	if (pgMgrHndl->fpm.isHeaderDirty) {
		//EDB_PRINT_I("FLUSH Header");
		writePMHeader(pgMgrHndl, true);
	}

	/*
	 * Can defer flushing header till close or next flush
	 */
	fflush(pgMgrHndl->fpm.fd_ptr);

	if (!pgMgrHndl->isIOBlockMode)
		_OS_flushFat();
}

void edb_pm_clearCache(EDB_Hndl_t pgMgrHndl){
	fpm_clearCache(pgMgrHndl);
}

EDB_result_t edb_pm_operatinalState(EDB_Hndl_t pgMgrHndl) {
	return pgMgrHndl->isInBadSate ? EDB_PM_FAILED_BAD_STATE : EDB_OKAY;
}


uint32_t edb_getPageCount(EDB_Hndl_t pgMgrHndl) {
	uint32_t fLen;

	fseek(pgMgrHndl->fpm.fd_ptr, 0, SEEK_END);
	fLen = ftell(pgMgrHndl->fpm.fd_ptr);
	return fOffsetToPage(fLen);
}

bool fpm_spaceAvailable(EDB_Hndl_t pgMgrHndl) {
	return pgMgrHndl->fpm.s.maxPageCount == 0 || pgMgrHndl->fpm.s.usedPageCount < pgMgrHndl->fpm.s.maxPageCount;
}

#endif
