/*
 * edb_lib.c
 *
 */

#include "config.h"
#if INCLUDE_EDB

#include "string.h"
#include "stdlib.h"

#include "lib/assert.h"
#include <edb/pageMgr/pm.h>

#include "edb_lib.h"
#include <edb/pageMgr/pm_prv.h>


//#include "lib/filePageManager/valueAccess.h"
//#include "lib/filePageManager/pageAccess.h"


#define OP_STATUS_CREATE(error, count) \
	((EDB_OperationResult_t) { (error), (count) } \
	)
#define OP_STATUS_INITIALIZE \
	((EDB_OperationResult_t) { EDB_UNKNOWN, 0 } \
	)
#define OP_STATUS_ERROR(error) \
	((EDB_OperationResult_t) { (error), 0 } \
	)
#define OP_STATUS_OK(count) \
	((EDB_OperationResult_t) { EDB_OKAY, (count) } \
	)

//static bool test_predicate2(edb_crsHndl_t cursor, kvp_key_t key);

void EDB_register() {

}

EDB_result_t EDB_open(EDB_Hndl_t* edbHndlPtr, edb_pm_openArgsPtr_t fpmOpenArgs) {
	EDB_result_t err;
	EDB_Hndl_t edbHndl;
	uint32_t allocSz;
	uint32_t fNameLen = strlen(fpmOpenArgs->fileName) + 1;

	EDB_PRINT_I("Opening %s", fpmOpenArgs->fileName);

	//_ASSERT_DEBUG(*edbHndlPtr == NULL);
	if (*edbHndlPtr == NULL) {
		allocSz = sizeof(EDB_t);
		allocSz += fNameLen + 1;

		edbHndl = (EDB_Hndl_t) _OS_MAlloc(allocSz);
		if (edbHndl == NULL) {
			return EDB_PM_OUT_OF_MEMORY;
		}
		memset(edbHndl, 0, sizeof(EDB_t));
		edbHndl->fpm.fileName = (char*) ((uint8_t*) edbHndl + sizeof(EDB_t));
		edbHndl->isAllocated = true;

	} else {
		edbHndl = *edbHndlPtr;
		memset(edbHndl, 0, sizeof(EDB_t));
		edbHndl->isAllocated = false;
		edbHndl->fpm.fileName = _OS_MAlloc(fNameLen + 1);
	}

	strncpy(edbHndl->fpm.fileName, fpmOpenArgs->fileName, fNameLen);

	if (edb_pm_open(edbHndl, fpmOpenArgs))
		return EDB_ERROR;


#if EDB_ENABLE_X
	dll_init(&edbHndl->tables, 16);
	edb_trns_init(edbHndl);

	if (fpmOpenArgs->useWAL) {
		err = edb_wal_init(edbHndl);
	}

	EDB_xHndl_t x;
	EDB_trns_start(edbHndl, &x, EDB_X_ANY);
	if ((err = edb_preOp(x)))
		return err;

	err = edb_tables_load(edbHndl);
	if (err && err != EDB_NOT_FOUND) {
		edb_postOp(x);
		EDB_trns_cancel(x);
		EDB_close(edbHndl);
		return err;
	}
//	_ASSERT_DEBUG(edbHndl->isNewFile || (!edbHndl->isNewFile && err != EDB_NOT_FOUND));

//	if(EDB_getTable(edbHndl, 1) == NULL)
//		err = EDB_addTable(edbHndl, 1);
	edb_postOp(x);
	EDB_trns_commit(x);

	/*
	 * We need to sync the WAL to the DB
	 * This is needed do to the WALs inability to function
	 * With out a valid header with resources lookup pointing to the WAL
	 * Maybe a fixed WAL location or other solution would help.
	 * But since the resouce location never moves it will work
	 */
	if (edbHndl->isNew) {
		edb_wal_sync(edbHndl);
	}
	//^^
#endif
	if (edbHndl->needsRecovery) {

	}

	if (edbHndl->needsRecovery) {
		//edb_recover(edbHndl);
	}
	*edbHndlPtr = edbHndl;
	return EDB_OKAY;
}

#if EDB_ENABLE_X
EDB_result_t edb_preOp(EDB_xHndl_t x) {
	EDB_result_t r;
	EDB_Hndl_t edbHndl;

	edbHndl = x->edbHndl;

	if ((r = edb_pm_operatinalState(edbHndl)))
		return r;

	edb_trns_enter(x);

	return r;
}
void edb_postOp(EDB_xHndl_t x) {
	edb_trns_exit(x);
}
#endif

EDB_result_t EDB_flush(EDB_Hndl_t edbHndl) {
	//BPT_flush(&edbHndl->vIndex);
	edb_pm_flush(edbHndl);
	return EDB_OKAY;
}
void EDB_close(EDB_Hndl_t edbHndl) {

#if EDB_ENABLE_X
	/*edb_tables_unload called BPT flush, since BPT maintains
	 * its own buffer we done know if x.active->.isModfiy if the BPT root node
	 * is modified.
	 * So Lets flush the BPT root node to insure all pages are ready for commit
	 */
	edb_tables_unload(edbHndl);

	if (edbHndl->x.active != NULL) { // && edbHndl->x.active->.isModfiy
		//_ASSERT_DEBUG(edbHndl->x.active->isAutoX);
		EDB_trns_cancel(edbHndl->x.active);
	}

	edb_wal_sync(edbHndl);
	edb_wal_deinit(edbHndl);
	edb_trns_deinit(edbHndl);
	edbHndl->pgAcEx.enableWrite = false;
#endif
	//BPT_flush(&edbHndl->vIndex);
	EDB_PRINT_I("Closed Name: %s", edbHndl->fpm.fileName);

	edb_pm_close(edbHndl);
	if (edbHndl->isAllocated) {
		_OS_Free(edbHndl);
	} else {
		_OS_Free(edbHndl->fpm.fileName);
	}
}

#if EDB_ENABLE_X
EDB_result_t EDB_openValue(EDB_xHndl_t x, edb_tblHndl_t tbl, kvp_key_t key, FPM_Object_t* valueHndl) {
	EDB_result_t r;
	bpt_cursor_t cursor;
	EDB_Hndl_t fpm;

	fpm = tbl->edbHndl;

	if ((r = edb_preOp(x)))
		return r;

	r = BPT_findKey(&tbl->rowIndex->bptContext, &key, &cursor);

	if (r == (EDB_result_t) BPT_NOT_FOUND) {
		pgIdx_t pgIdx;
		if ((r = edb_val_add(fpm, key, &pgIdx))) {
			goto done;
		}
		if ((r = BPT_insertKey(&tbl->rowIndex->bptContext, &key, pgIdx))) {
			goto done;
		}

		r = edb_obj_open(fpm, valueHndl, pgIdx);

	} else if (r == (EDB_result_t) BPT_OKAY) {
		r = edb_obj_open(fpm, valueHndl, cursor.valueOffset);
	}

	done: //
	edb_postOp(x);
	return r;
}

EDB_result_t EDB_openValue_ByPage(EDB_xHndl_t x, FPM_Object_t* valueHndl, pgIdx_t pgIdx) {
	EDB_result_t r;

	if ((r = edb_preOp(x)))
		return r;

	r = edb_obj_open(x->edbHndl, valueHndl, pgIdx);

	edb_postOp(x);
	return r;
}
EDB_result_t EDB_set(EDB_xHndl_t x, edb_tblHndl_t tbl, kvp_key_t key, buf_t* value) {
	EDB_result_t r;
	bpt_cursor_t cursor;
	pgIdx_t newOffset;
	EDB_Hndl_t fpm;

	fpm = tbl->edbHndl;

	if ((r = edb_preOp(x)))
		return r;

//offset	= ION_FILE_NULL;
	r = BPT_findKey(&tbl->rowIndex->bptContext, &key, &cursor);

	if (r == (EDB_result_t) BPT_NOT_FOUND) {
		if ((r = FPM_insertValue(fpm, key, value->data, value->dataLen, &newOffset)))
			goto done;

		r = BPT_insertKey(&tbl->rowIndex->bptContext, &key, newOffset);

	} else if (r == (EDB_result_t) BPT_OKAY) {
		FPM_Object_t valueHndl;
		if ((r = edb_obj_open(fpm, &valueHndl, cursor.valueOffset)))
			goto done;

		if ((r = edb_obj_write(&valueHndl, value)))
			goto done;

		edb_obj_truncate(&valueHndl);

		edb_obj_close(&valueHndl);

#if FPM_WRITE_ON_UPDATE
		newOffset = cursor.valueOffset;
		if ((r = FPM_updateValue(fpm, cursor.valueOffset, key, value->data, value->dataLen, &newOffset)))
		goto done;

		if (newOffset != cursor.valueOffset) {
			if ((r = BPT_updateKey(&tbl->rowIndex->bptContext, &key, newOffset)))
			goto done;
		}
#endif
	}

	done: //
	edb_postOp(x);
	return r;
}

EDB_result_t EDB_append(EDB_xHndl_t x, edb_tblHndl_t tbl, kvp_key_t key, buf_t* value) {
	EDB_result_t r;
	bpt_cursor_t cursor;
	EDB_Hndl_t fpm;

	fpm = tbl->edbHndl;

	if ((r = edb_preOp(x)))
		return r;

	r = BPT_findKey(&tbl->rowIndex->bptContext, &key, &cursor);

	if (r == (EDB_result_t) BPT_NOT_FOUND) {
		r = EDB_NOT_FOUND;
	} else if (r == (EDB_result_t) BPT_OKAY) {
		r = FPM_appendValue(fpm, cursor.valueOffset, key, value->data, value->dataLen);

	}

	edb_postOp(x);
	return r;
}

EDB_result_t EDB_get(EDB_xHndl_t x, edb_tblHndl_t tbl, kvp_key_t key, buf_t* value) {
	EDB_result_t r;
	bpt_cursor_t cursor;

	if ((r = edb_preOp(x)))
		return r;

	r = (EDB_result_t) BPT_findKey(&tbl->rowIndex->bptContext, &key, &cursor);

	if ((EDB_result_t) BPT_NOT_FOUND == r) {
		r = EDB_NOT_FOUND;
	} else if (r == (EDB_result_t) BPT_OKAY) {
		r = edb_get_byValuePgIdx(x, tbl, cursor.valueOffset, value);
	}

	edb_postOp(x);
	return r;
}

EDB_result_t EDB_get_byValuePgIdx(EDB_xHndl_t x, edb_tblHndl_t tbl, pgIdx_t valuePageIdx, buf_t* value) {
	EDB_result_t r;

	if ((r = edb_preOp(x)))
		return r;

	r = edb_get_byValuePgIdx(x, tbl, valuePageIdx, value);

	edb_postOp(x);
	return r;
}

EDB_result_t edb_get_byValuePgIdx(EDB_xHndl_t x, edb_tblHndl_t tbl, pgIdx_t valuePageIdx, buf_t* value) {
	EDB_result_t r;
	FPM_Object_t valHndl;
	EDB_Hndl_t fpm;

	fpm = tbl->edbHndl;

	if ((r = edb_obj_open(fpm, &valHndl, valuePageIdx)))
		goto done;

	uint32_t vLen = FPM_objectLen(&valHndl);
	r = EDB_OKAY;

	if (value->data == NULL) {
		value->data = _OS_MAlloc(vLen);
		value->dataLen = vLen;
		if (value->data == NULL) {
			r = EDB_PM_OUT_OF_MEMORY;
		}
	} else if (value->dataLen < vLen) {
		r = EDB_ERROR_BUFFER_UNDER_SIZED;
	}

	if (r == EDB_OKAY) {
		r = edb_obj_read(&valHndl, value->data, vLen);
	}
	edb_obj_close(&valHndl);

	done: //
	return r;
}

EDB_result_t EDB_getPage(EDB_xHndl_t x, edb_tblHndl_t tbl, uint32_t key, buf_t* value) {
	EDB_result_t r;
	bpt_cursor_t cursor;
	EDB_Hndl_t fpm;

	fpm = tbl->edbHndl;

	if ((r = edb_preOp(x)))
		return r;

	r = BPT_findKey(&tbl->rowIndex->bptContext, &key, &cursor);

	if (r == (EDB_result_t) BPT_NOT_FOUND) {
		r = EDB_NOT_FOUND;
	} else if (r == (EDB_result_t) BPT_OKAY) {
		if ((r = FPM_getValueSize(fpm, cursor.valueOffset, &value->dataLen)))
			goto done;

		r = FPM_getValueData(fpm, cursor.valueOffset, &value->data);
	}

	done: //
	edb_postOp(x);
	return r;
}

EDB_result_t EDB_exists(EDB_xHndl_t x, edb_tblHndl_t tbl, uint32_t key) {
	EDB_result_t r;
	bpt_cursor_t cursor;

	if ((r = edb_preOp(x)))
		return r;

	r = BPT_findKey(&tbl->rowIndex->bptContext, &key, &cursor);

	if (r == (EDB_result_t) BPT_NOT_FOUND) {
		r = EDB_NOT_FOUND;
	} else if (r == (EDB_result_t) BPT_OKAY) {
		r = EDB_OKAY;
	}

	edb_postOp(x);
	return r;
}

EDB_result_t EDB_delete(EDB_xHndl_t x, edb_tblHndl_t tbl, uint32_t key) {
	EDB_result_t r;
	pgIdx_t offset;
	pageType_e pgType;
	bpt_cursor_t cursor;
	EDB_Hndl_t fpm;

	fpm = tbl->edbHndl;

	if ((r = edb_preOp(x)))
		return r;

//	if ((r = edb_trns_check(tbl->edbHndl)))
//		return r;

	r = BPT_findKey(&tbl->rowIndex->bptContext, &key, &cursor);

	if (r == (EDB_result_t) BPT_NOT_FOUND) {
		r = EDB_NOT_FOUND;
		goto done;
	} else if (r != (EDB_result_t) BPT_OKAY) {
		goto done;
	}

	pgType = edb_pm_getPageType(fpm, cursor.valueOffset);

	if ((r = FPM_deleteValue(fpm, cursor.valueOffset)))
		goto done;

	if (pgType == EDB_PM_PT_VALUE) {
		r = BPT_deleteKey(&tbl->rowIndex->bptContext, &key, &offset);
	}

	done: //
	edb_postOp(x);
	return r;
}
#endif
void EDB_check(EDB_Hndl_t edbHndl) {

	FPM_printStatus(edbHndl);

#if EDB_ENABLED_NESTED_X
	EDB_checkTbls((EDB_xHndl_t)edbHndl->x.allActive.head);
#endif

}

#endif

