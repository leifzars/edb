/*
 * index.c
 *
 */

#include <edb/config.h>
#if INCLUDE_EDB
#if EDB_ENABLE_X

#include "stdlib.h"

#include "lib/doubleLinkList.h"

#include <edb/pageMgr/pm_prv.h>

static EDB_result_t edb_indexs_read(EDB_Hndl_t edbHndl, edb_tblHndl_t tbl);
static EDB_result_t edb_indexs_write(EDB_Hndl_t edbHndl, edb_tblHndl_t tbl);

EDB_result_t edb_indexs_load(EDB_Hndl_t edbHndl, edb_tblHndl_t tbl) {
	return edb_indexs_read(edbHndl, tbl);
}

void edb_indexs_unload(edb_tblHndl_t tbl) {
	edb_tblIdxHndl_t IdxHndl;

	top: IdxHndl = dll_pop(&(tbl->indexs));
	if (IdxHndl) {
		BPT_close(&IdxHndl->bptContext);
		_OS_Free(IdxHndl);
		goto top;
	}
}

EDB_result_t EDB_tblIdxGet(EDB_xHndl_t x, edb_tblHndl_t tbl, EDB_IdxID_t id, edb_tblIdxHndl_t* out_idx) {
	edb_trns_enter(x);

	if ((*out_idx = edb_getIndex(tbl, id))) {
		edb_postOp(x);
		return EDB_OKAY;
	}

	edb_postOp(x);
	return EDB_OKAY_DOES_NOT_EXIST;
}

EDB_result_t EDB_tblIdxAdd(EDB_xHndl_t x, edb_tblHndl_t tbl, EDB_IdxID_t id, bool allowDupKeys,
		edb_tblIdxHndl_t* out_idx) {
	EDB_result_t r;
	edb_trns_enter(x);

	if ((r = edb_addIndex(tbl->edbHndl, tbl, id, allowDupKeys, out_idx))) {
		goto done;
	}

	done: //
	edb_postOp(x);
	return EDB_OKAY;
}

//BPT_hndl_t handle, void *key, bpt_exAddress_t *rec

EDB_result_t EDB_idxAddKey(EDB_xHndl_t x, edb_tblIdxHndl_t idx, void *key, bpt_exAddress_t rec) {
	EDB_result_t r;

	if ((r = edb_preOp(x)))
		return r;

	r = (EDB_result_t) BPT_insertKey(&idx->bptContext, key, rec);

	edb_postOp(x);
	return r;
}

EDB_result_t EDB_idxDeleteKey(EDB_xHndl_t x, edb_tblIdxHndl_t idx, void *key, bpt_exAddress_t *rec) {
	EDB_result_t r;

	if ((r = edb_preOp(x)))
		return r;

	r = (EDB_result_t) BPT_deleteKey(&idx->bptContext, key, rec);

	edb_postOp(x);
	return r;
}

EDB_result_t EDB_idxGetValue(EDB_xHndl_t x, edb_tblIdxHndl_t idx, uint32_t key, pgIdx_t* val) {
	EDB_result_t r;

	if ((r = edb_preOp(x)))
		return r;

	r = (EDB_result_t) BPT_get(&idx->bptContext, key, val);

	edb_postOp(x);
	return r;
}
bool EDB_idxExists_match(EDB_xHndl_t x, edb_tblIdxHndl_t idx, uint32_t key, pgIdx_t val) {
	EDB_result_t r;

	if ((r = edb_preOp(x)))
		return r;

	r = (EDB_result_t) BPT_getKeyValue(&idx->bptContext, key, val);

	edb_postOp(x);
	return r == (EDB_result_t) BPT_OKAY;
}

EDB_result_t edb_addIndex(EDB_Hndl_t edbHndl, edb_tblHndl_t tbl, EDB_IdxID_t id,
bool allowDupKeys, edb_tblIdxHndl_t* out_idxHdnl) {
	edb_tblIdxHndl_t nIdxHndl;
	EDB_result_t err;

	nIdxHndl = edb_getIndex(tbl, id);
	if (nIdxHndl != NULL)
		return EDB_ALREADY_EXISTS;

	nIdxHndl = _OS_MAlloc(sizeof(edb_tblIdx_t));
	if (nIdxHndl == NULL)
		return EDB_PM_OUT_OF_MEMORY;

	memset(nIdxHndl, 0, sizeof(edb_tblIdx_t));

	nIdxHndl->c.id = id;
	nIdxHndl->tbl = tbl;
	nIdxHndl->c.allowDupKeys = allowDupKeys;

	if((err = (BPT_Result_e) BPT_open_UInt32(&nIdxHndl->bptContext, edbHndl, nIdxHndl->c.rootPageIdxs, allowDupKeys))){
		_OS_Free(nIdxHndl);
		return err;
	}

	dll_add(&tbl->indexs, &nIdxHndl->indexs);

	edb_indexs_write(edbHndl, tbl);

	*out_idxHdnl = nIdxHndl;

	return BPT_OKAY;
}

edb_tblIdxHndl_t edb_getIndex(edb_tblHndl_t tbl, EDB_IdxID_t id) {
	edb_tblIdxHndl_t IdxHndl;

	FOR_EACH_dll_ELEMENT2(&(tbl->indexs), el)
	{
		IdxHndl = (edb_tblIdxHndl_t) el;
		if (IdxHndl->c.id == id)
			return IdxHndl;
	}
	return NULL;

}

EDB_result_t edb_indexs_flush(EDB_Hndl_t edbHndl, edb_tblHndl_t tbl, bool andInvalidate) {
	edb_tblIdxHndl_t IdxHndl;
	EDB_result_t err;

	FOR_EACH_dll_ELEMENT2(&(tbl->indexs), el)
	{
		IdxHndl = (edb_tblIdxHndl_t) el;

		err = BPT_flush(&IdxHndl->bptContext);
		if (andInvalidate)
			BPT_cacheInvalidate(&IdxHndl->bptContext);
	}
	return err;
}

void edb_indexs_cacheInvalidate(EDB_Hndl_t edbHndl, edb_tblHndl_t tbl) {
	edb_tblIdxHndl_t IdxHndl;

	FOR_EACH_dll_ELEMENT2(&(tbl->indexs), el)
	{
		IdxHndl = (edb_tblIdxHndl_t) el;
		BPT_cacheInvalidate(&IdxHndl->bptContext);
	}
}

EDB_result_t edb_indexs_write(EDB_Hndl_t edbHndl, edb_tblHndl_t tbl) {
	EDB_result_t err;
	FPM_Object_t o;

	buf_t wrtMem;
	EDB_TblIdxCfg_t indexConfig;
	pgIdx_t idxTop;

	if (tbl->c.indexsPgIdx == 0) {
		err = edb_obj_add(edbHndl, EDB_PM_PT_INDEX_DEF, &idxTop, tbl->tableDefPgIdx, NULL, NULL);

		tbl->c.indexsPgIdx = idxTop;
	}

	if ((err = edb_obj_open(edbHndl, &o, tbl->c.indexsPgIdx)))
		return err;
	//FPM_valueScanToStart
	edb_tblIdxHndl_t IdxHndl;

	wrtMem.dataLen = sizeof(EDB_TblIdxCfg_t);
	wrtMem.data = &indexConfig;

	FOR_EACH_dll_ELEMENT2(&(tbl->indexs), el)
	{
		IdxHndl = (edb_tblIdxHndl_t) el;

		indexConfig = IdxHndl->c;
		err = edb_obj_write(&o, &wrtMem);
	}

	edb_obj_close(&o);
	return EDB_OKAY;
}

EDB_result_t edb_indexs_read(EDB_Hndl_t edbHndl, edb_tblHndl_t tbl) {
	EDB_result_t err;
	FPM_Object_t o;

	uint8_t buff[sizeof(EDB_TblIdxCfg_t)];
	edb_tblIdx_t* tblIdxCfg;

	_ASSERT_DEBUG(tbl->indexs.length == 0);

	if (tbl->c.indexsPgIdx == 0) {
		return EDB_NOT_FOUND;
	}

	err = edb_obj_open(edbHndl, &o, tbl->c.indexsPgIdx);

	if (!err) {
		while (!(err = edb_obj_read(&o, (void*) buff, sizeof(EDB_TblIdxCfg_t)))) {

			tblIdxCfg = _OS_MAlloc(sizeof(edb_tblIdx_t));
			if (tblIdxCfg == NULL)
				return EDB_PM_OUT_OF_MEMORY;

			memset(tblIdxCfg, 0, sizeof(edb_tblIdx_t));

			memcpy(&tblIdxCfg->c, buff, sizeof(EDB_TblIdxCfg_t));

			tblIdxCfg->tbl = tbl;
			dll_add(&tbl->indexs, &tblIdxCfg->indexs);

			err = (BPT_Result_e) BPT_open_UInt32(&tblIdxCfg->bptContext, edbHndl, tblIdxCfg->c.rootPageIdxs,
					tblIdxCfg->c.allowDupKeys);

		};
		edb_obj_close(&o);

		if (err == EDB_PM_EXCEEDS_EOF_VALUE)
			err = EDB_OKAY;
	}
	return err;
}

EDB_result_t EDB_idxCount(EDB_xHndl_t x, edb_tblIdxHndl_t idx, uint32_t* out_count) {
	EDB_result_t r;

	if ((r = edb_preOp(x)))
		return r;

	r = edb_idxCount(idx, out_count);

	edb_postOp(x);
	return r;
}

EDB_result_t edb_idxCount(edb_tblIdxHndl_t idx, uint32_t* out_count) {
	EDB_result_t r;

	r = (EDB_result_t) BPT_count(&idx->bptContext, out_count);
	return r;
}

void edb_checkIndexs(edb_tblHndl_t tbl) {
	edb_tblIdxHndl_t IdxHndl;
	//BPT_Result_e r;
	//uint32_t idxEntryCt_last;
	//uint32_t idxEntryCt;
	uint32_t idxCt;

	//idxEntryCt = idxEntryCt_last = 0;
	idxCt = 0;

	FOR_EACH_dll_ELEMENT2(&(tbl->indexs), el)
	{
		IdxHndl = (edb_tblIdxHndl_t) el;
		EDB_PRINT_I("  Idx %d) Prim %d Dups %d UsdPgs %d", IdxHndl->c.id, IdxHndl->c.id == tbl->rowIndex->c.id,
				IdxHndl->c.allowDupKeys, IdxHndl->c.usedPageCount);

		BPT_cursor_t cur;
		BPT_record_t record;
		uint32_t minKey = 0, minId = 0, maxKey = 0, maxId = 0;

		int ct = 0;

		BPT_cursorInit_All(&IdxHndl->bptContext, &cur, BPT_PT_ACS);

		while (BPT_cursorNext(&cur, &record) == BPT_CS_CURSOR_ACTIVE) {
			if (ct == 0) {
				minKey = record.key;
				minId = record.valuePgIdx;
			} else {
				maxKey = record.key;
				maxId = record.valuePgIdx;
			}
			ct++;
		}

//		idxEntryCt_last = idxEntryCt;
//		idxEntryCt = ct;
//
//		if(idxCt){
//			_ASSERT(idxEntryCt_last == idxEntryCt);
//		}

		EDB_PRINT_I("      Ct %d  min: %d @ %d max: %d @ %d", ct, minKey, minId, maxKey, maxId);

		idxCt++;
	}

}

BPT_Result_e EDB_idx_initSearch_All(EDB_xHndl_t x, edb_tblIdxHndl_t idx, BPT_crsHndl_t cursor, BPT_order_t order) {
	BPT_Result_e r;
	if ((r = edb_preOp(x)))
		return r;

	r = BPT_cursorInit_All(&idx->bptContext, cursor, order);

	edb_postOp(x);
	return r;
}
BPT_Result_e EDB_idx_initSearch_Range(EDB_xHndl_t x, edb_tblIdxHndl_t idx, BPT_crsHndl_t cursor, kvp_key_t lower,
		kvp_key_t upper, BPT_order_t order) {
	BPT_Result_e r;
	if ((r = edb_preOp(x)))
		return r;

	r = BPT_cursorInit_Range(&idx->bptContext, cursor, lower, upper, order);

	edb_postOp(x);
	return r;
}
BPT_Result_e EDB_idx_initSearch_Equal(EDB_xHndl_t x, edb_tblIdxHndl_t idx, BPT_crsHndl_t cursor, kvp_key_t value) {
	BPT_Result_e r;
	if ((r = edb_preOp(x)))
		return r;

	r = BPT_cursorInit_Equal(&idx->bptContext, cursor, value );

	edb_postOp(x);
	return r;
}
BPT_cursorStatus_t EDB_idx_findNext(EDB_xHndl_t x, BPT_crsHndl_t cursor, BPT_record_t *out_record) {
	BPT_cursorStatus_t r;
	if ((r = edb_preOp(x)))
		return BPT_CS_END_OF_RESULTS_ON_ERR;

	r = BPT_cursorNext(cursor, out_record);

	edb_postOp(x);
	return r;
}

BPT_cursorStatus_t EDB_idx_prevPeriod(EDB_xHndl_t x, BPT_hndl_t handle, BPT_crsHndl_t cursor, kvp_key_t validLow,
		kvp_key_t validHigh) {
	BPT_cursorStatus_t r;
	if ((r = edb_preOp(x)))
		return BPT_CS_END_OF_RESULTS_ON_ERR;

	r = BPT_cursorPrevPeriod(handle, cursor, validLow, validHigh);

	edb_postOp(x);
	return r;
}
#endif
#endif
