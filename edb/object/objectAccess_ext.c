/*
 * objectAccess_RA.c
 *
 */

#include <edb/config.h>
#if INCLUDE_EDB

#if EDB_ENABLE_X
#define TRANS_ENTER		edb_trns_enter(o->xHndl);
#define TRANS_EXIT		edb_trns_exit(o->xHndl);
#else
#define TRANS_ENTER
#define TRANS_EXIT
#endif

#include "lib/assert.h"

#include <edb/pageMgr/pm_prv.h>
#include <edb/page/pageAccess.h>
#include "objectPageHeader.h"

static EDB_result_t edb_obj_scanToNextPage(FPM_Object_t* valHndl, bool extend);
static EDB_result_t edb_obj_scanToPrevPage(FPM_Object_t* valHndl);
static void calcValuePageAndOffset(FPM_Object_t* valHndl, uint32_t offset, uint32_t* out_pageNumber,
		uint32_t* out_pageOffset);
static uint32_t calcValuePageStart(FPM_Object_t* valHndl, uint32_t valuePageNumber);

#if EDB_ENABLE_X
static void objectOpen(EDB_Hndl_t pgMgrHndl, EDB_xHndl_t xHndl, FPM_Object_t* valHndl, void* page);
EDB_result_t FPM_objectCreate(EDB_xHndl_t xHndl, FPM_Object_t* valHndl, pgIdx_t* out_valuePgIdx) {
	EDB_result_t r;
	void* page;
	EDB_Hndl_t edbHndl;

	edbHndl = xHndl->edbHndl;

	edb_trns_enter(xHndl);

	if ((r = fpm_getEmptyPage(edbHndl, EDB_PM_PT_VALUE, out_valuePgIdx, &page)))
		goto done;

	objectOpen(edbHndl, xHndl, valHndl, page);

	done: //
	edb_trns_exit(xHndl);

	return r;
}

EDB_result_t FPM_objectOpen(EDB_xHndl_t xHndl, FPM_Object_t* valHndl, pgIdx_t valuePgIdx) {
	EDB_result_t r;
	EDB_Hndl_t edbHndl;

	edbHndl = xHndl->edbHndl;

	edb_trns_enter(xHndl);

	r = edb_obj_open(edbHndl, valHndl, valuePgIdx);

	edb_trns_exit(xHndl);
	return r;
}



#else
static void objectOpen(EDB_Hndl_t pgMgrHndl, FPM_Object_t* valHndl, void* page);
EDB_result_t FPM_objectCreate(EDB_Hndl_t edbHndl, FPM_Object_t* valHndl, pgIdx_t* out_valuePgIdx) {
	EDB_result_t r;
	void* page;

	if (r = fpm_getEmptyPage(edbHndl, EDB_PM_PT_VALUE, out_valuePgIdx, &page))
	goto done;

	objectOpen(edbHndl, valHndl, page);

	done: //
	return r;
}

EDB_result_t FPM_objectOpen(EDB_Hndl_t edbHndl, FPM_Object_t* valHndl, pgIdx_t valuePgIdx) {
	EDB_result_t r;
	void* page;

	if ((r = FPM_getPage(edbHndl, valuePgIdx, true, &page)))
	goto done;

	objectOpen(edbHndl, valHndl, page);

	done: //

	return r;
}
#endif


EDB_result_t edb_obj_open(EDB_Hndl_t edbHndl, FPM_Object_t* valHndl, pgIdx_t valuePgIdx) {
	EDB_result_t r;
	void* page;

	if ((r = FPM_getPage(edbHndl, valuePgIdx, true, &page)))
		goto done;

#if EDB_ENABLE_X
	objectOpen(edbHndl, edbHndl->x.active, valHndl, page);
#else
	objectOpen(edbHndl, valHndl, page);
#endif
	done: //

	return r;
}


#if EDB_ENABLE_X
void objectOpen(EDB_Hndl_t pgMgrHndl, EDB_xHndl_t xHndl, FPM_Object_t* valHndl, void* page) {
#else
	void objectOpen(EDB_Hndl_t pgMgrHndl, FPM_Object_t* valHndl, void* page) {
#endif
	fpm_objectHndl_t o = (fpm_objectHndl_t) valHndl;

	memset(o, 0, sizeof(fpm_object_t));

	o->oph = (FPM_objPgHeader_t*) page;
	_ASSERT_DEBUG(
			fpm_pgTypeInfos[o->oph->base.base.type].isObject
					&& !fpm_pgTypeInfos[o->oph->base.base.type].isObjectOverFlow);

	valHndl->objPgHeaderSize = fpm_pgTypeInfos[o->oph->base.base.type].headerSize;
	valHndl->objOfPgHeaderSize = fpm_pgTypeInfos[o->oph->base.base.type + 1].headerSize;

	valHndl->objPgDataSpace = _OS_getPageSize() - valHndl->objPgHeaderSize;
	valHndl->objOfPgDataSpace = _OS_getPageSize() - valHndl->objOfPgHeaderSize;

	o->pgMgrHndl = pgMgrHndl;
#if EDB_ENABLE_X
	o->xHndl = xHndl;
#endif
	o->activePage = (FPM_objBasePgHeader_t*) o->oph;

	o->cursor.offset = 0;
	o->cursor.pgOffset = 0;
	o->cursor.ptr = (void*) o->oph + valHndl->objPgHeaderSize;
	o->cursor.len = valHndl->objPgDataSpace;
}

void EDB_obj_close(FPM_Object_t* valHndl) {
	fpm_objectHndl_t o = (fpm_objectHndl_t) valHndl;

#if EDB_ENABLE_X
	EDB_xHndl_t xHndl = o->xHndl;
	edb_trns_enter(xHndl);
#endif
	edb_obj_close(valHndl);

#if EDB_ENABLE_X
	edb_trns_exit(xHndl);
#endif

}
void edb_obj_close(FPM_Object_t* valHndl) {
	fpm_objectHndl_t o = (fpm_objectHndl_t) valHndl;

	_ASSERT_DEBUG(o->activePage != NULL);

	if (o->oph != NULL)
		fpm_returnPage(o->pgMgrHndl, o->oph, FPM_PGBUF_NONE);

	if (o->activePage != NULL && (FPM_objBasePgHeader_t*) o->oph != o->activePage)
		fpm_returnPage(o->pgMgrHndl, o->activePage, FPM_PGBUF_NONE);

	memset(o, 0, sizeof(fpm_object_t));
}

void FPM_objectScanToStart(FPM_Object_t* valHndl) {
	fpm_objectHndl_t o = (fpm_objectHndl_t) valHndl;

	_ASSERT_DEBUG(o->activePage != NULL);

	TRANS_ENTER
	;

	if (o->activePage != NULL && (FPM_objBasePgHeader_t*) o->oph != o->activePage)
		fpm_returnPage(o->pgMgrHndl, o->activePage, FPM_PGBUF_NONE);

	o->activePage = (FPM_objBasePgHeader_t*) o->oph;

	o->activePageValueOffset = 0;
	o->cursor.offset = 0;
	o->cursor.pgOffset = 0;
	o->cursor.ptr = (void*) o->oph + valHndl->objPgHeaderSize;
	o->cursor.len = valHndl->objPgDataSpace;

	TRANS_EXIT
	;
}


EDB_result_t EDB_obj_scanToEnd(FPM_Object_t* valHndl) {
	EDB_result_t r;
	fpm_objectHndl_t o = (fpm_objectHndl_t) valHndl;

#if EDB_ENABLE_X
	edb_trns_enter(o->xHndl);
#endif
	r = edb_obj_scanToEnd(valHndl);

#if EDB_ENABLE_X
	edb_trns_exit(o->xHndl);
#endif

	return r;
}

EDB_result_t edb_obj_scanToEnd(FPM_Object_t* valHndl) {
	EDB_result_t r;
	fpm_objectHndl_t o = (fpm_objectHndl_t) valHndl;

	uint32_t valuePageNumber;
	uint32_t valuePageOffset;

	r = EDB_OKAY;

	_ASSERT_DEBUG(o->activePage != NULL);

	if (o->oph->base.prevPage == 0) {
		_ASSERT_DEBUG(o->activePage == (FPM_objBasePgHeader_t* ) o->oph);
		_ASSERT_DEBUG(o->oph->objLen < valHndl->objPgDataSpace);

		o->cursor.offset = o->oph->objLen;
		o->cursor.ptr = (void*) o->activePage + valHndl->objPgHeaderSize + o->cursor.offset;
		o->cursor.len = valHndl->objPgDataSpace - o->oph->objLen;
		o->cursor.pgOffset = o->oph->objLen;
	} else {
		if (o->oph->base.prevPage != o->activePage->base.pageNumber) {
			if ((FPM_objBasePgHeader_t*) o->oph != o->activePage)
				fpm_returnPage(o->pgMgrHndl, o->activePage, FPM_PGBUF_NONE);
			if ((r = FPM_getPage(o->pgMgrHndl, o->oph->base.prevPage, true, (void**) &o->activePage)))
				goto done;
		}

		calcValuePageAndOffset(valHndl, o->oph->objLen, &valuePageNumber, &valuePageOffset);

		o->cursor.offset = o->oph->objLen;

		if (valuePageOffset) {
			o->activePageValueOffset = calcValuePageStart(valHndl, valuePageNumber);
			o->cursor.ptr = (void*) o->activePage + sizeof(valueOverFlowPageHeader_t) + valuePageOffset;
			o->cursor.len = valHndl->objOfPgDataSpace - valuePageOffset;
			o->cursor.pgOffset = valuePageOffset;
		} else {
			//points to next un allocated value over flow page
			o->activePageValueOffset = calcValuePageStart(valHndl, valuePageNumber - 1);
			o->cursor.ptr = NULL;
			o->cursor.len = 0;
			o->cursor.pgOffset = 0;
		}
	}

	done: //
	return r;
}

EDB_result_t edb_obj_scanToNextPage(FPM_Object_t* valHndl, bool extend) {
	fpm_objectHndl_t o = (fpm_objectHndl_t) valHndl;
	EDB_result_t r;

	_ASSERT_DEBUG(o->activePage != NULL);
//_ASSERT_DEBUG(!(o->valueMasterPgIdx != 0 && extend == true));

	if (o->activePage->nextPage == 0) {
		valueOverFlowPageHeader_t* nvopg;
		pgIdx_t nPageIdx;

		if (!extend) {
			r = EDB_PM_EXCEEDS_EOF_VALUE;
			goto done;
		}

		if ((r = fpm_getEmptyPage(o->pgMgrHndl, EDB_PM_PT_VALUE_OVERFLOW, &nPageIdx, (void**) &nvopg)))
			goto done;

		fpm_objPg_append(o->oph, o->activePage, &nvopg->base);

		if (o->activePage != (FPM_objBasePgHeader_t*) o->oph) {
			fpm_returnPage(o->pgMgrHndl, (void*) o->activePage, FPM_PGBUF_DIRTY);
			fpm_setPageDirty(o->pgMgrHndl, (void*) o->oph);
		} else
			fpm_setPageDirty(o->pgMgrHndl, (void*) o->activePage);

		o->activePage = (FPM_objBasePgHeader_t*) nvopg;

	} else {
		pgIdx_t nextOverFlow;
		nextOverFlow = o->activePage->nextPage;

		if (o->activePage != (FPM_objBasePgHeader_t*) o->oph)
			fpm_returnPage(o->pgMgrHndl, o->activePage, FPM_PGBUF_NONE);

		if ((r = FPM_getPage(o->pgMgrHndl, nextOverFlow, true, (void**) &o->activePage)))
			goto done;
	}

	_ASSERT_DEBUG(o->activePage->base.type == EDB_PM_PT_VALUE_OVERFLOW);

	if (o->activePageValueOffset == 0) {
		o->activePageValueOffset += valHndl->objPgDataSpace;
	} else {
		o->activePageValueOffset += valHndl->objOfPgDataSpace;
	}

	o->cursor.ptr = (void*) o->activePage + sizeof(valueOverFlowPageHeader_t);
	o->cursor.offset = o->activePageValueOffset;
	o->cursor.len = valHndl->objOfPgDataSpace;
	o->cursor.pgOffset = 0;

	done: //
	return r;
}

EDB_result_t edb_obj_scanToPrevPage(FPM_Object_t* valHndl) {
	fpm_objectHndl_t o = (fpm_objectHndl_t) valHndl;
	EDB_result_t r;

	r = EDB_OKAY;

	_ASSERT_DEBUG(o->activePage != NULL);
//_ASSERT_DEBUG(!(o->valueMasterPgIdx != 0 && extend == true));

	if (o->activePage->prevPage == 0) {
		return EDB_PM_EXCEEDS_EOF_VALUE;
	}

	pgIdx_t prevOverFlow;
	prevOverFlow = o->activePage->prevPage;

	if (o->activePage != (FPM_objBasePgHeader_t*) o->oph)
		fpm_returnPage(o->pgMgrHndl, o->activePage, FPM_PGBUF_NONE);

	if (prevOverFlow == o->oph->base.base.pageNumber) {
		o->activePage = &o->oph->base;
	} else {
		if ((r = FPM_getPage(o->pgMgrHndl, prevOverFlow, true, (void**) &o->activePage)))
			goto done;
	}

	if (o->activePage->ownerPage == 0) {
		_ASSERT_DEBUG(o->activePage->base.type == EDB_PM_PT_VALUE);
		o->activePageValueOffset -= valHndl->objPgDataSpace;
		o->cursor.pgOffset = valHndl->objPgDataSpace;
		_ASSERT_DEBUG(o->cursor.offset == o->activePageValueOffset + valHndl->objPgDataSpace);
	} else {
		_ASSERT_DEBUG(o->activePage->base.type == EDB_PM_PT_VALUE_OVERFLOW);
		o->activePageValueOffset -= valHndl->objOfPgDataSpace;
		o->cursor.pgOffset = valHndl->objOfPgDataSpace;
		_ASSERT_DEBUG(o->cursor.offset == o->activePageValueOffset + valHndl->objOfPgDataSpace);
	}

	o->cursor.ptr = (void*) o->activePage + _OS_getPageSize();
	o->cursor.len = 0;

	done: //
	return r;
}

EDB_result_t FPM_objectScanToOffset(FPM_Object_t* valHndl, uint32_t offset) {
	EDB_result_t r;
	fpm_objectHndl_t o = (fpm_objectHndl_t) valHndl;

	r = EDB_PM_EXCEEDS_EOF_VALUE;

	_ASSERT_DEBUG(o->activePage != NULL);

	TRANS_ENTER
	;

	if (offset > o->oph->objLen)
		return EDB_PM_EXCEEDS_EOF_VALUE;

	if (offset < o->activePageValueOffset) {
		FPM_objectScanToStart(valHndl);

	} else if (offset == o->oph->objLen) {
		if ((r = edb_obj_scanToEnd(valHndl)))
			goto done;

		uint32_t offsetIntoRegion = o->cursor.offset - offset;
		o->cursor.offset = offset;
		o->cursor.ptr -= offsetIntoRegion;
		o->cursor.len += offsetIntoRegion;
		r = EDB_OKAY;
		goto done;
	} else {
		uint32_t offsetIntoRegion = o->cursor.offset - o->activePageValueOffset;
		o->cursor.offset = o->activePageValueOffset;
		o->cursor.ptr -= offsetIntoRegion;
		o->cursor.len += offsetIntoRegion;
	}

	do {
		if (offset >= o->cursor.offset && offset < o->cursor.offset + o->cursor.len) {

			uint32_t offsetIntoRegion = offset - o->activePageValueOffset;
			o->cursor.offset = offset;
			o->cursor.ptr += offsetIntoRegion;
			o->cursor.len -= offsetIntoRegion;
			r = EDB_OKAY;
			goto done;
		}

		if ((r = edb_obj_scanToNextPage(valHndl, false)))
			goto done;

	} while (1);

	r = EDB_PM_EXCEEDS_EOF_VALUE;
	done: //
	TRANS_EXIT
	;
	return r;
}

EDB_result_t EDB_obj_truncate(FPM_Object_t* valHndl) {
	EDB_result_t r;
	fpm_objectHndl_t o = (fpm_objectHndl_t) valHndl;

#if EDB_ENABLE_X
	edb_trns_enter(o->xHndl);
#endif
	r = edb_obj_truncate(valHndl);

#if EDB_ENABLE_X
	edb_trns_exit(o->xHndl);
#endif

	return r;
}

EDB_result_t edb_obj_truncate(FPM_Object_t* valHndl) {
	EDB_result_t r;
	fpm_objectHndl_t o = (fpm_objectHndl_t) valHndl;
	FPM_objBasePgHeader_t* nxtPg;
	FPM_objBasePgHeader_t* pg;

	r = EDB_OKAY;

	if (o->oph->objLen == o->cursor.offset)
		return EDB_OKAY;

	pg = o->activePage;
	while (pg->nextPage) {
		if ((r = FPM_getPage(o->pgMgrHndl, pg->nextPage, true, (void**) &nxtPg)))
			goto done;

		if (o->activePage != pg) {
			edb_freePage(o->pgMgrHndl, pg->base.pageNumber);
		}

		pg = nxtPg;
	}
	if (o->activePage != pg) {
		edb_freePage(o->pgMgrHndl, pg->base.pageNumber);
		fpm_objPg_setTail(o->oph, o->activePage);
		fpm_setPageDirty(o->pgMgrHndl, (void*) o->activePage);
	}

	o->oph->objLen = o->cursor.offset;

	fpm_setPageDirty(o->pgMgrHndl, (void*) o->oph);

	done: //
	return r;
}

EDB_result_t EDB_obj_write(FPM_Object_t* valHndl, buf_t* buf) {
	EDB_result_t r;
	fpm_objectHndl_t o = (fpm_objectHndl_t) valHndl;

#if EDB_ENABLE_X
	edb_trns_enter(o->xHndl);
#endif
	r = edb_obj_write(valHndl, buf);

#if EDB_ENABLE_X
	edb_trns_exit(o->xHndl);
#endif

	return r;
}
EDB_result_t edb_obj_write(FPM_Object_t* valHndl, buf_t* buf) {
	EDB_result_t r = EDB_OKAY;
	fpm_objectHndl_t o = (fpm_objectHndl_t) valHndl;
	uint32_t bytesToWrite;
	uint32_t bytesWriten = 0;
	uint32_t requieredFreePages;
	void* srcPtr = buf->data;

	_ASSERT_DEBUG(o->activePage != NULL);
//_ASSERT_DEBUG(o->activePage->base.pageNumber != 1);

	if (buf->dataLen == 0)
		return EDB_OKAY;

	requieredFreePages = fpm_calcRequieredAdditionalPages(o->oph->objLen, o->cursor.offset, buf->dataLen);
	if (requieredFreePages && requieredFreePages > edb_pm_availablePages(o->pgMgrHndl)) {
		return EDB_PM_IO_ERROR_FULL;
	}

	while (1) {
		bytesToWrite = buf->dataLen - bytesWriten;
		if (bytesToWrite > o->cursor.len)
			bytesToWrite = o->cursor.len;

		if (bytesToWrite) {
			memcpy(o->cursor.ptr, srcPtr, bytesToWrite);

			srcPtr += bytesToWrite;
			bytesWriten += bytesToWrite;
			o->cursor.pgOffset += bytesToWrite;

			fpm_setPageDirty(o->pgMgrHndl, (void*) o->activePage);
		}

		if (bytesWriten < buf->dataLen) {
			if ((r = edb_obj_scanToNextPage(valHndl, true))) {
				if (r != EDB_OKAY && r != EDB_PM_EXCEEDS_EOF_VALUE)
					goto done;
				break;
			}
		} else {

			break;
		}
	};

	o->cursor.offset += bytesToWrite;
	o->cursor.ptr += bytesToWrite;
	o->cursor.len -= bytesToWrite;
	o->cursor.pgOffset = 0;

	if (o->cursor.len == 0)
		o->cursor.ptr = NULL;

	if ((o->cursor.offset) > o->oph->objLen) {
		o->oph->objLen = o->cursor.offset;
		fpm_setPageDirty(o->pgMgrHndl, (void*) o->oph);
	}

	done: //

	return r;
}

EDB_result_t EDB_obj_read(FPM_Object_t* valHndl, void* buf, uint32_t len) {
	EDB_result_t r;
	fpm_objectHndl_t o = (fpm_objectHndl_t) valHndl;

#if EDB_ENABLE_X
	edb_trns_enter(o->xHndl);
#endif
	r = edb_obj_read(valHndl, buf, len);

#if EDB_ENABLE_X
	edb_trns_exit(o->xHndl);
#endif
	return r;
}

EDB_result_t edb_obj_read(FPM_Object_t* valHndl, void* buf, uint32_t len) {
	EDB_result_t r = EDB_OKAY;
	fpm_objectHndl_t o = (fpm_objectHndl_t) valHndl;
	void* destPtr = buf;
	uint32_t bytesToRead;
	uint32_t bytesRead = 0;

	_ASSERT_DEBUG(o->activePage != NULL);

	if (len == 0 || buf == NULL)
		return EDB_OKAY;

	if (o->cursor.offset + len > o->oph->objLen)
		return EDB_PM_EXCEEDS_EOF_VALUE;

	while (1) { //bytesRead < len) {
		bytesToRead = len - bytesRead;
		if (bytesToRead > o->cursor.len)
			bytesToRead = o->cursor.len;

		if (bytesToRead > 0) {
			memcpy(destPtr, o->cursor.ptr, bytesToRead);

			destPtr += bytesToRead;
			bytesRead += bytesToRead;
			o->cursor.pgOffset += bytesToRead;
		}

		if (bytesRead < len) {
			if ((r = edb_obj_scanToNextPage(valHndl, false)))
				break;
		} else {
			break;
		}
	};

	o->cursor.offset += bytesToRead;
	o->cursor.ptr += bytesToRead;
	o->cursor.len -= bytesToRead;

	return r;
}
EDB_result_t EDB_obj_readR(FPM_Object_t* valHndl, void* buf, uint32_t len) {
	EDB_result_t r;
		fpm_objectHndl_t o = (fpm_objectHndl_t) valHndl;

	#if EDB_ENABLE_X
		edb_trns_enter(o->xHndl);
	#endif
		r = edb_obj_readR(valHndl, buf, len);

	#if EDB_ENABLE_X
		edb_trns_exit(o->xHndl);
	#endif
		return r;
}
EDB_result_t edb_obj_readR(FPM_Object_t* valHndl, void* buf, uint32_t len) {
	EDB_result_t r = EDB_OKAY;
	fpm_objectHndl_t o = (fpm_objectHndl_t) valHndl;
	void* destPtr = (uint8_t*) buf + len;
	uint32_t bytesToRead;
	uint32_t bytesRead = 0;

	_ASSERT_DEBUG(o->activePage != NULL);

	if (len == 0 || buf == NULL)
		return EDB_OKAY;

	if (o->cursor.offset - len > o->oph->objLen)
		return EDB_PM_EXCEEDS_EOF_VALUE;

	while (1) { //bytesRead < len) {
		bytesToRead = len - bytesRead;
		if (bytesToRead > o->cursor.pgOffset)
			bytesToRead = o->cursor.pgOffset;

		if (bytesToRead > 0) {
			destPtr -= bytesToRead;
			o->cursor.ptr -= bytesToRead;
			o->cursor.pgOffset -= bytesToRead;
			o->cursor.offset -= bytesToRead;
			o->cursor.len += bytesToRead;

			memcpy(destPtr, o->cursor.ptr, bytesToRead);

			bytesRead += bytesToRead;

		}

		if (bytesRead < len) {
			if ((r = edb_obj_scanToPrevPage(valHndl)))
				break;
		} else {
			break;
		}
	};

	return r;
}


#if EDB_ENABLE_X
EDB_result_t EDB_obj_add(EDB_xHndl_t xHndl, pageType_e pgType, pgIdx_t* out_valuePgIdx, pgIdx_t ownerPgIdx, FPM_objCallBack_t cb, void* arg) {
	EDB_result_t r;
#if EDB_ENABLE_X
	edb_trns_enter(xHndl);
#endif
	r = edb_obj_add(xHndl->edbHndl, pgType, out_valuePgIdx, ownerPgIdx,cb,arg);
#if EDB_ENABLE_X
	edb_trns_exit(xHndl);
#endif
	return r;
}
#endif

EDB_result_t edb_obj_add(EDB_Hndl_t pgMgrHndl, pageType_e pgType, pgIdx_t* out_valuePgIdx, pgIdx_t ownerPgIdx, FPM_objCallBack_t cb, void* arg) {
	EDB_result_t err;
	uint32_t pageIdx;
	void* page;
	FPM_objBasePgHeader_t* oph;

	if ((err = fpm_getEmptyPage(pgMgrHndl, pgType, &pageIdx, &page)))
		return err;

	oph = (FPM_objBasePgHeader_t*) page;
	oph->ownerPage = ownerPgIdx;

	if(cb){
		cb(pgMgrHndl, page, arg);
	}

	fpm_returnPage(pgMgrHndl, page, FPM_PGBUF_DIRTY);

	*out_valuePgIdx = pageIdx;

	return EDB_OKAY;
}
uint32_t FPM_objectLen(FPM_Object_t* valHndl) {
	fpm_objectHndl_t o = (fpm_objectHndl_t) valHndl;
	return o->oph->objLen;
}
uint32_t FPM_objectPageIndex(FPM_Object_t* valHndl) {
	fpm_objectHndl_t o = (fpm_objectHndl_t) valHndl;
	return o->oph->base.base.pageNumber;
}
uint32_t calcValuePageStart(FPM_Object_t* valHndl, uint32_t valuePageNumber) {
	if (valuePageNumber == 0)
		return 0;
	else
		return valHndl->objPgDataSpace + ((valuePageNumber - 1) * valHndl->objOfPgDataSpace);
}

void calcValuePageAndOffset(FPM_Object_t* valHndl, uint32_t offset, uint32_t* out_pageNumber, uint32_t* out_pageOffset) {
	if (offset < valHndl->objPgDataSpace) {
		*out_pageNumber = 0;
		*out_pageOffset = offset;
	} else {
		*out_pageNumber = ((offset - valHndl->objPgDataSpace) / valHndl->objOfPgDataSpace) + 1;
		*out_pageOffset = (offset - valHndl->objPgDataSpace) % valHndl->objOfPgDataSpace;
	}
}

#endif
