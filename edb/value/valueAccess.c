/*
 * valueAccess.c
 *
 */

#include <edb/config.h>
#if INCLUDE_EDB


#include "lib/assert.h"

#include <edb/pageMgr/pm_prv.h>
#include <edb/page/pageAccess.h>
#include "../pageHeaders/valuePageHeader.h"

//static EDB_result_t insertValue_empty(FPM_Hndl_t pgMgrHndl, pgIdx_t masterPgIdx, uint32_t key, pgIdx_t* out_valuePgIdx);

static EDB_result_t insertValue(EDB_Hndl_t pgMgrHndl, pgIdx_t masterPgIdx, uint32_t key, void* value, uint32_t valueLen,
		pgIdx_t* out_pgIdx);

static EDB_result_t writeValue(EDB_Hndl_t pgMgrHndl, pgIdx_t masterPgIdx, uint32_t key, void* value, uint32_t valueLen,
		pgIdx_t* inout_valuePgIdx);

EDB_result_t getValuePage(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx, valuePageHeader_t** out_vph, void** out_value);

static void writeValueOverflowPage(EDB_Hndl_t pgMgrHndl, pgIdx_t nPgIdx, void* emptyPageBuffer, pgIdx_t valueHeadPgIdx,
		void* valueChunk, uint32_t valueChunkLen, uint16_t matchOnPageOverflow, void* previousPage);

static void writeValuePage(EDB_Hndl_t pgMgrHndl, void* emptyPageBuffer, pgIdx_t masterPgIdx, uint32_t key, void* value,
		uint32_t valueLen, uint16_t matchOnPageOverflow);

//static void updateValuePageHeaderOnOverflowPageAppend(valuePageHeader_t* vph, pgIdx_t newAppendedPgIdx,
//		uint32_t appendedLen);

static uint32_t appendValueOverflowPage(valuePageHeader_t* vph, valueOverFlowPageHeader_t* voph, void* appendValue,
		uint32_t appendValueLen);
static uint32_t appendValuePage(valuePageHeader_t* vph, void* appendValue, uint32_t appendValueLen);

EDB_result_t FPM_insertValue(EDB_Hndl_t pgMgrHndl, uint32_t key, void* value, uint32_t valueLen, pgIdx_t* out_pgIdx) {
	return insertValue(pgMgrHndl, 0, key, value, valueLen, out_pgIdx);
}

EDB_result_t FPM_updateValue(EDB_Hndl_t pgMgrHndl, pgIdx_t masterPgIdx, uint32_t key, void* value, uint32_t valueLen,
		pgIdx_t* out_newPgIdx) {
	EDB_result_t err;

	if ((err = insertValue(pgMgrHndl, 0, key, value, valueLen, out_newPgIdx))) {
		return err;
	}

	err = fpm_freeValue(pgMgrHndl, masterPgIdx);
	return err;

}

EDB_result_t FPM_vm_setValue(EDB_Hndl_t pgMgrHndl, pgIdx_t masterPgIdx, uint32_t key, void* value, uint32_t valueLen) {
	EDB_result_t err;
	pgIdx_t newValuePgIdx;
	err = insertValue(pgMgrHndl, masterPgIdx, key, value, valueLen, &newValuePgIdx);

	return err;
}

EDB_result_t FPM_appendValue(EDB_Hndl_t pgMgrHndl, pgIdx_t valuePgIdx, uint32_t key, void* value, uint32_t valueLen) {
	pgIdx_t inout_valuePgIdx = valuePgIdx;

	return writeValue(pgMgrHndl, 0, key, value, valueLen, &inout_valuePgIdx);
}

EDB_result_t insertValue(EDB_Hndl_t pgMgrHndl, pgIdx_t masterPgIdx, uint32_t key, void* value, uint32_t valueLen,
		pgIdx_t* out_pgIdx) {
	*out_pgIdx = 0;
	return writeValue(pgMgrHndl, masterPgIdx, key, value, valueLen, out_pgIdx);
}

EDB_result_t writeValue(EDB_Hndl_t pgMgrHndl, pgIdx_t masterPgIdx, uint32_t key, void* value, uint32_t valueLen,
		pgIdx_t* inout_valuePgIdx) {
	EDB_result_t err;
	uint32_t pageIdx;
	void* previousPage = NULL;
	void* page;
	void* srcPtr = value;
	pgIdx_t headPageIdx = *inout_valuePgIdx;
	uint32_t bytesWritten = 0;
	uint32_t bytesToWrite;
	pageType_e pgType;
	uint16_t matchOnPageOverflow;
	valuePageHeader_t* vph = NULL;
	uint32_t requieredFreePages;

	if (pgMgrHndl->isInBadSate)
		return EDB_PM_FAILED_BAD_STATE;
	if (pgMgrHndl->isReadOnly)
		return EDB_PM_FAILED_READ_ONLY;
	if (valueLen == 0)
		return EDB_PM_LENGTH_CAN_NOT_BE_ZERO; // or can it

	if (headPageIdx > 0) { //Existing Value Page so must be Append operation
		if ((err = FPM_getPage(pgMgrHndl, headPageIdx, true, (void**) &vph)))
			return err;

		matchOnPageOverflow = 0;

		requieredFreePages = fpm_calcRequieredAdditionalPages(vph->base.objLen, 0, valueLen);
		if (requieredFreePages > edb_pm_availablePages(pgMgrHndl)) {
			return EDB_PM_IO_ERROR_FULL;
		}

		_ASSERT(vph->key == key);
		_ASSERT(vph->base.base.base.type == EDB_PM_PT_VALUE);

		if (vph->base.base.prevPage == 0) {
			page = (void*) vph;
		} else {
			if ((err = FPM_getPage(pgMgrHndl, vph->base.base.prevPage, true, (void**) &page)))
				return err;
			_ASSERT(((valueOverFlowPageHeader_t* ) page)->base.base.type == EDB_PM_PT_VALUE_OVERFLOW);
		}

		{
			bytesToWrite = valueLen - bytesWritten;

			if (page == vph) {
				bytesWritten += appendValuePage(vph, srcPtr, bytesToWrite);
			} else {
				bytesWritten += appendValueOverflowPage(vph, (valueOverFlowPageHeader_t*) page, srcPtr, bytesToWrite);
			}
			previousPage = page;
		}
	} else {
		//requieredFreePages = fpm_calcRequieredAdditionalPages(vph->base.objLen, 0, valueLen);
		requieredFreePages = fpm_calcRequieredAdditionalPages(0, 0, valueLen);
		if (requieredFreePages > edb_pm_availablePages(pgMgrHndl)) {
			return EDB_PM_IO_ERROR_FULL;
		}
		matchOnPageOverflow = 0;

		pgType = EDB_PM_PT_VALUE;
	}

	while (bytesWritten < valueLen) {

		if ((err = fpm_getEmptyPage(pgMgrHndl, pgType, &pageIdx, &page)))
			return err;

		if (!headPageIdx) {
			bytesToWrite = valueLen < FPM_VALUE_PAGE_DATA_SIZE ? valueLen : FPM_VALUE_PAGE_DATA_SIZE;

			writeValuePage(pgMgrHndl, page, masterPgIdx, key, srcPtr, bytesToWrite, matchOnPageOverflow);
			headPageIdx = pageIdx;
			vph = (valuePageHeader_t*) page;
		} else {
			bytesToWrite = valueLen - bytesWritten;
			if (bytesToWrite > FPM_VALUE_PAGE_OVERFLOW_DATA_SIZE)
				bytesToWrite = FPM_VALUE_PAGE_OVERFLOW_DATA_SIZE;

			fpm_objPg_append(&vph->base, (FPM_objBasePgHeader_t*) previousPage, (FPM_objBasePgHeader_t*) page);

			writeValueOverflowPage(pgMgrHndl, pageIdx, page, headPageIdx, srcPtr, bytesToWrite, matchOnPageOverflow,
					previousPage);

			vph->base.objLen += bytesToWrite;
			//updateValuePageHeaderOnOverflowPageAppend(vph, pageIdx, bytesToWrite);
		}

		srcPtr = (uint8_t*) srcPtr + bytesToWrite;
		bytesWritten += bytesToWrite;

		if (previousPage) {
			if (previousPage != (void*) vph) {
				fpm_returnPage(pgMgrHndl, previousPage, FPM_PGBUF_DIRTY);
			}
		}
		previousPage = page;
		pgType = EDB_PM_PT_VALUE_OVERFLOW;
	}

	fpm_returnPage(pgMgrHndl, (void*) vph, FPM_PGBUF_DIRTY);

	if (page && page != vph) {
		fpm_returnPage(pgMgrHndl, page, FPM_PGBUF_DIRTY);
	}

	*inout_valuePgIdx = headPageIdx;

	return EDB_OKAY;
}

typedef struct {
	uint32_t key;
} objArgs_key_t;


void valueInset_HeaderSet(EDB_Hndl_t pgMgrHndl, void* page, objArgs_key_t* key) {
	valuePageHeader_t* vph;
	vph = (valuePageHeader_t*) page;

	vph->masterPgIdx = 0;
	vph->key = key->key;
}
EDB_result_t edb_val_add(EDB_Hndl_t pgMgrHndl, uint32_t key, pgIdx_t* out_valuePgIdx) {
	objArgs_key_t args;
	args.key = key;
	return edb_obj_add(pgMgrHndl, EDB_PM_PT_VALUE, out_valuePgIdx, 0, (FPM_objCallBack_t)valueInset_HeaderSet, &args);
//return insertValue_empty(pgMgrHndl, 0, key, out_valuePgIdx);
}
//
//EDB_result_t insertValue_empty(FPM_Hndl_t pgMgrHndl, pgIdx_t masterPgIdx, uint32_t key, pgIdx_t* out_valuePgIdx) {
//	EDB_result_t err;
//	uint32_t pageIdx;
//	void* page;
//	valuePageHeader_t* vph;
//
//	if ((err = fpm_markDirty(pgMgrHndl)))
//		return err;
//
//	if ((err = fpm_getEmptyPage(pgMgrHndl, FPM_PAGE_TYPE_VALUE, &pageIdx, &page)))
//		return err;
//
//	vph = (valuePageHeader_t*) page;
//
//	vph->masterPgIdx = masterPgIdx;
//	vph->key = key;
//#if FPM_VALUE_INCLUDE_VALID_FIELDS
//	vph->base.base.validChildKey = (uint16_t) pgMgrHndl->s.nextOperationId;
//#endif
//
//	fpm_returnPage(pgMgrHndl, page, FPM_PGBUF_DIRTY);
//
//	*out_valuePgIdx = pageIdx;
//
//	return EDB_OKAY;
//}

uint32_t appendValuePage(valuePageHeader_t* vph, void* appendValue, uint32_t appendValueLen) {
//vph = (valuePageHeader_t*) appendPage;
	uint32_t bytesToWrite = appendValueLen;
	void* appendDest;
	uint32_t remainingValueSpace = FPM_VALUE_PAGE_DATA_SIZE - vph->base.objLen;
	uint32_t usedValueSpace = FPM_VALUE_PAGE_DATA_SIZE - remainingValueSpace;

	_ASSERT(vph->base.objLen < FPM_VALUE_PAGE_DATA_SIZE);
	if (remainingValueSpace == 0)
		return 0;

	if (bytesToWrite > remainingValueSpace)
		bytesToWrite = remainingValueSpace;

	appendDest = (void*) ((uint8_t*) vph + sizeof(valuePageHeader_t) + usedValueSpace);
	memcpy(appendDest, appendValue, bytesToWrite);

	vph->base.objLen += appendValueLen;

	return bytesToWrite;
}
//uint32_t getRequieredFreePages(valuePageHeader_t* vph, uint32_t offset, uint32_t len) {
//	uint32_t len2WrtRem = len;
//	uint32_t pgCt;
//	uint32_t neededOverFlowPages = 0;
//	if(vph == NULL){
//
//	}else{
//		_ASSERT_DEBUG(vph->base.objLen >= offset);
//		uint32_t overwritLen = vph->base.objLen - offset;
//
//	}
//	if (vph == NULL || (vph && vph->base.objLen == 0)) {
//		if (len < FPM_VALUE_PAGE_DATA_SIZE)
//			len2WrtRem = 0;
//		else
//			len2WrtRem -= FPM_VALUE_PAGE_DATA_SIZE;
//		pgCt = 1;
//
//	} else {
//
//		_ASSERT_DEBUG(vph->base.objLen >= offset);
//		uint32_t overwritLen = vph->base.objLen - offset;
//		len2WrtRem = len - overwritLen;
//		uint32_t remainingValueSpace = (vph->base.objLen - FPM_VALUE_PAGE_DATA_SIZE) % FPM_VALUE_PAGE_OVERFLOW_DATA_SIZE;
//		if (len2WrtRem < remainingValueSpace)
//			len2WrtRem = 0;
//		else
//			len2WrtRem -= remainingValueSpace;
//	}
//
//	if (len2WrtRem) {
//		neededOverFlowPages = (len2WrtRem / FPM_VALUE_PAGE_OVERFLOW_DATA_SIZE) + 1;
//		pgCt += neededOverFlowPages;
//
//	}
//
//	return pgCt;
//}
//

/*
 * Assumes first page already exists. So first FPM_VALUE_PAGE_DATA_SIZE bytes require no pages
 */
uint32_t fpm_calcRequieredAdditionalPages(uint32_t objLen, uint32_t offset, uint32_t appendLen) {
	uint32_t len2WrtRem = appendLen;
	uint32_t pgCt = 0;
	uint32_t neededOverFlowPages = 0;

	uint32_t updateLen = objLen - offset;
	uint32_t remainingPageSpace;

	if (updateLen >= appendLen)
		return 0; // only updating no append;

	len2WrtRem = appendLen - updateLen;

	if (objLen < FPM_VALUE_PAGE_DATA_SIZE) {
		remainingPageSpace = FPM_VALUE_PAGE_DATA_SIZE - objLen;
		if (len2WrtRem < remainingPageSpace)
			len2WrtRem = 0;
		else
			len2WrtRem -= remainingPageSpace;
	}

	if (len2WrtRem) {
		uint32_t remainingValueSpace;
		remainingValueSpace = (objLen - FPM_VALUE_PAGE_DATA_SIZE) % FPM_VALUE_PAGE_OVERFLOW_DATA_SIZE;

		if (len2WrtRem < remainingValueSpace)
			len2WrtRem = 0;
		else
			len2WrtRem -= remainingValueSpace;

		if (len2WrtRem) {
			neededOverFlowPages = (len2WrtRem / FPM_VALUE_PAGE_OVERFLOW_DATA_SIZE) + 1;
			pgCt += neededOverFlowPages;
		}
	}

	return pgCt;
}

uint32_t appendValueOverflowPage(valuePageHeader_t* vph, valueOverFlowPageHeader_t* voph, void* appendValue,
		uint32_t appendValueLen) {
//vph = (valuePageHeader_t*) appendPage;
	uint32_t bytesToWrite = appendValueLen;
	uint32_t remainingValueSpace = (vph->base.objLen - FPM_VALUE_PAGE_DATA_SIZE) % FPM_VALUE_PAGE_OVERFLOW_DATA_SIZE;
	uint32_t usedValueSpace = FPM_VALUE_PAGE_OVERFLOW_DATA_SIZE - remainingValueSpace;
	void* appendDest;

	_ASSERT(remainingValueSpace < FPM_VALUE_PAGE_OVERFLOW_DATA_SIZE);
	if (remainingValueSpace == 0)
		return 0;

	if (bytesToWrite > remainingValueSpace)
		bytesToWrite = remainingValueSpace;

	appendDest = (void*) ((uint8_t*) voph + sizeof(valueOverFlowPageHeader_t) + usedValueSpace);
	memcpy(appendDest, appendValue, bytesToWrite);

	vph->base.objLen += appendValueLen;

	return bytesToWrite;
}

EDB_result_t FPM_getValuePage(EDB_Hndl_t pgMgrHndl, EDB_valueAccessInfo_t* accessInfo) {
	EDB_result_t err;
	valuePageHeader_t* vph;

	if (pgMgrHndl->isInBadSate)
		return EDB_PM_FAILED_BAD_STATE;

	if ((err = getValuePage(pgMgrHndl, accessInfo->pgIdx, &vph, &accessInfo->valueBuffer))) {
		return err;
	}
	if (accessInfo->pgIdx)
		_ASSERT(vph->key == accessInfo->key);
	accessInfo->pageBuffer = (void*) vph;
	accessInfo->valueSize = vph->base.objLen;
	return err;
}

EDB_result_t getValuePage(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx, valuePageHeader_t** out_vph, void** out_value) {
	EDB_result_t err;
	edb_pm_pageHeader_t* ph;
	valuePageHeader_t* vph;

	if ((err = FPM_getPage(pgMgrHndl, pgIdx, true, (void**) &ph)))
		return err;

	vph = (valuePageHeader_t*) ph;

	if (vph->base.base.base.type == EDB_PM_PT_VALUE) {
		_ASSERT_DEBUG(vph->base.objLen < _OS_getPageSize() - sizeof(valuePageHeader_t));
	}

	*out_vph = vph;
	*out_value = (void*) ((uint8_t*) vph + sizeof(valuePageHeader_t));
	return EDB_OKAY;
}

void FPM_returnValuePage(EDB_Hndl_t pgMgrHndl, void* value) {
	void* page;

	page = (void*) ((uint8_t*) value - sizeof(valuePageHeader_t));
	fpm_returnPage(pgMgrHndl, page, FPM_PGBUF_NONE);
}

EDB_result_t FPM_getValueSize(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx, uint32_t* out_size) {
	valuePageHeader_t* vph;
	void* valuePtr;
	EDB_result_t err;

	if (pgMgrHndl->isInBadSate)
		return EDB_PM_FAILED_BAD_STATE;

	if ((err = getValuePage(pgMgrHndl, pgIdx, &vph, &valuePtr))) {
		return err;
	}

	*out_size = vph->base.objLen;

	fpm_returnPage(pgMgrHndl, (void*) vph, FPM_PGBUF_NONE);
	return EDB_OKAY;
}

EDB_result_t FPM_getValueData(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx, void** dataBufferPtr) {
	valuePageHeader_t* vph;
	void* valuePtr;
	EDB_result_t err;

	if (pgMgrHndl->isInBadSate)
		return EDB_PM_FAILED_BAD_STATE;

	if ((err = getValuePage(pgMgrHndl, pgIdx, &vph, &valuePtr))) {
		return err;
	}

	*dataBufferPtr = valuePtr;

	fpm_returnPage(pgMgrHndl, (void*) vph, FPM_PGBUF_NONE);
	return EDB_OKAY;
}

EDB_result_t FPM_deleteValue(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx) {
	if (pgMgrHndl->isInBadSate)
		return EDB_PM_FAILED_BAD_STATE;
	if (pgMgrHndl->isReadOnly)
		return EDB_PM_FAILED_READ_ONLY;

	return fpm_freeValue(pgMgrHndl, pgIdx);
}

EDB_result_t fpm_freeValue(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx) {
	FPM_objBasePgHeader_t* vph;
	uint32_t currentPage = pgIdx;
	uint32_t nextPage;
	EDB_result_t err;

	do {
		if ((err = FPM_getPage(pgMgrHndl, currentPage, true, (void**) &vph)))
			return err;

		_ASSERT_DEBUG(vph->base.type == EDB_PM_PT_VALUE || vph->base.type == EDB_PM_PT_VALUE_OVERFLOW);

		nextPage = vph->nextPage;
		fpm_returnPage(pgMgrHndl, (void*) vph, FPM_PGBUF_NONE);

		edb_freePage(pgMgrHndl, currentPage);
		currentPage = nextPage;
	} while (currentPage);

	return EDB_OKAY;
}

void writeValueOverflowPage(EDB_Hndl_t pgMgrHndl, pgIdx_t nPgIdx, void* emptyPageBuffer, pgIdx_t valueHeadPgIdx,
		void* valueChunk, uint32_t valueChunkLen, uint16_t matchOnPageOverflow, void* previousPage) {
	valueOverFlowPageHeader_t* newVoph;
	void* newPageValue;

	newVoph = (valueOverFlowPageHeader_t*) emptyPageBuffer;
	newPageValue = (void*) ((uint8_t*) emptyPageBuffer + sizeof(valueOverFlowPageHeader_t));

	newVoph->base.base.type = EDB_PM_PT_VALUE_OVERFLOW;

	memcpy(newPageValue, valueChunk, valueChunkLen);

}

//void updateValuePageHeaderOnOverflowPageAppend(valuePageHeader_t* vph, pgIdx_t newAppendedPgIdx, uint32_t appendedLen) {
//	vph->base.base.prevPage = newAppendedPgIdx;
//	vph->base.objLen += appendedLen;
//}

void writeValuePage(EDB_Hndl_t pgMgrHndl, void* emptyPageBuffer, pgIdx_t masterPgIdx, uint32_t key, void* value,
		uint32_t valueLen, uint16_t matchOnPageOverflow) {
	valuePageHeader_t* newVph;
	void* newPageValue;

	newVph = (valuePageHeader_t*) emptyPageBuffer;
	newPageValue = (void*) ((uint8_t*) emptyPageBuffer + sizeof(valuePageHeader_t));

//newVph->base.base.base.type = FPM_PAGE_TYPE_VALUE; //not needed, done on get emptypage
	newVph->key = key;
	newVph->base.objLen = valueLen;
	newVph->masterPgIdx = masterPgIdx;
	newVph->base.base.prevPage = 0;
	newVph->base.base.nextPage = 0;

	if (valueLen > FPM_VALUE_PAGE_DATA_SIZE)
		valueLen = FPM_VALUE_PAGE_DATA_SIZE;

	memcpy(newPageValue, value, valueLen);

}

#endif
