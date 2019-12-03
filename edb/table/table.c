/*
 * table.c
 *
 */

#include <edb/config.h>
#if INCLUDE_EDB
#if EDB_ENABLE_X

#include "stdlib.h"
#include "string.h"

#include "lib/doubleLinkList.h"

#include <edb/pageMgr/pm_prv.h>

static EDB_result_t tablesRead(EDB_Hndl_t edbHndl);
static EDB_result_t tablesWrite(EDB_Hndl_t edbHndl);
static EDB_result_t tablesLoadIndexs(EDB_Hndl_t edbHndl);

EDB_result_t edb_tables_load(EDB_Hndl_t edbHndl) {
	EDB_result_t err;
	if ((err = tablesRead(edbHndl)))
		return err;
	err = tablesLoadIndexs(edbHndl);
	return err;
}

void edb_tables_unload(EDB_Hndl_t edbHndl) {
	edb_tblHndl_t tblHndl;

	top: tblHndl = dll_pop(&(edbHndl->tables));
	if (tblHndl) {
		edb_indexs_unload(tblHndl);
		_OS_Free(tblHndl);
		goto top;
	}
}

EDB_result_t EDB_tableGetOrCreate(EDB_xHndl_t x, EDB_tblID_t id, edb_tblHndl_t* out_tblHndl) {
	EDB_result_t err;

	err = EDB_tableGet(x, id, out_tblHndl);
	if (err && err == EDB_OKAY_DOES_NOT_EXIST)
		err = EDB_tableAdd(x, id, out_tblHndl);

	return err;
}

EDB_result_t EDB_tableGet(EDB_xHndl_t x, EDB_tblID_t id, edb_tblHndl_t* out_tblHndl) {
	edb_tblHndl_t tbl;
	EDB_result_t err;
	EDB_Hndl_t edbHndl;

	if ((err = edb_preOp(x)))
		return err;

	edbHndl = x->edbHndl;

	if (!(tbl = EDB_getTable(edbHndl, id))) {
		edb_postOp(x);
		return EDB_OKAY_DOES_NOT_EXIST;
	}

	*out_tblHndl = tbl;

	edb_postOp(x);
	return EDB_OKAY;
}

EDB_result_t EDB_tableAdd(EDB_xHndl_t x, EDB_tblID_t id, edb_tblHndl_t* out_tblHndl) {
	edb_tblHndl_t tbl;
	EDB_result_t err;
	EDB_Hndl_t edbHndl;

	if ((err = edb_preOp(x)))
		return err;

	edbHndl = x->edbHndl;

	tbl = EDB_getTable(edbHndl, id);
	if (tbl != NULL)
		return EDB_ALREADY_EXISTS;

	tbl = _OS_MAlloc(sizeof(edb_tblIdx_t));
	if (tbl == NULL)
		return EDB_PM_OUT_OF_MEMORY;

	memset(tbl, 0, sizeof(edb_tblIdx_t));
	dll_init(&tbl->indexs, 16);
	tbl->c.id = id;
	tbl->edbHndl = edbHndl;

	if ((err = edb_addIndex(edbHndl, tbl, EDB_KEY_INDEX_ID, false, &tbl->rowIndex))) {
		_OS_Free(tbl);
		return err;
	}

	dll_add(&edbHndl->tables, &tbl->tables);

	tablesWrite(edbHndl);

	*out_tblHndl = tbl;

	edb_postOp(x);
	return EDB_OKAY;
}

edb_tblHndl_t EDB_getTable(EDB_Hndl_t edbHndl, EDB_tblID_t id) {
	edb_tblHndl_t tblHndl;

	FOR_EACH_dll_ELEMENT2(&(edbHndl->tables), el)
	{
		tblHndl = (edb_tblHndl_t) el;
		if (tblHndl->c.id == id)
			return tblHndl;
	}
	return NULL;

}

EDB_result_t edb_tables_cacheFlush(EDB_Hndl_t edbHndl, bool andInvalidate) {
	edb_tblHndl_t tblHndl;
	EDB_result_t err;
	FOR_EACH_dll_ELEMENT2(&(edbHndl->tables), el)
	{
		tblHndl = (edb_tblHndl_t) el;
		err = edb_indexs_flush(edbHndl, tblHndl, andInvalidate);
	}
	return err;
}

void edb_tables_cacheInvalidate(EDB_Hndl_t edbHndl) {
	edb_tblHndl_t tblHndl;
	FOR_EACH_dll_ELEMENT2(&(edbHndl->tables), el)
	{
		tblHndl = (edb_tblHndl_t) el;
		edb_indexs_cacheInvalidate(edbHndl, tblHndl);
	}
}

EDB_result_t tablesWrite(EDB_Hndl_t edbHndl) {
	EDB_result_t err;
	FPM_Object_t o;

	buf_t wrtMem;
	EDB_TblCfg_t tblCfg;
	pgIdx_t idxTop;
	edb_tblHndl_t tblHndl;

	idxTop = edb_getResourcePgIdx(edbHndl, FPM_R_TABLE_DEFS);
	if (idxTop == 0) {
		err = edb_obj_add(edbHndl, EDB_PM_PT_TABLE_DEF, &idxTop, 0, NULL, NULL);

		edb_setResourcePgIdx(edbHndl, FPM_R_TABLE_DEFS, idxTop);
	}

	if ((err = edb_obj_open(edbHndl, &o, idxTop)))
		return err;
	//FPM_valueScanToStart

	wrtMem.dataLen = sizeof(EDB_TblCfg_t);
	wrtMem.data = &tblCfg;

	FOR_EACH_dll_ELEMENT2(&(edbHndl->tables), el)
	{
		tblHndl = (edb_tblHndl_t) el;

		tblCfg = tblHndl->c;
		err = edb_obj_write(&o, &wrtMem);
	}

	edb_obj_close(&o);
	return EDB_OKAY;
}

EDB_result_t tablesLoadIndexs(EDB_Hndl_t edbHndl) {
	EDB_result_t err;
	edb_tblHndl_t tblHndl;

	FOR_EACH_dll_ELEMENT2(&(edbHndl->tables), el)
	{
		tblHndl = (edb_tblHndl_t) el;
		err = edb_indexs_load(edbHndl, tblHndl);

		tblHndl->rowIndex = edb_getIndex(tblHndl, EDB_KEY_INDEX_ID);
	}
	return err;
}

EDB_result_t tablesRead(EDB_Hndl_t edbHndl) {
	EDB_result_t err;
	FPM_Object_t o;

	uint8_t buff[sizeof(EDB_TblCfg_t)];
	edb_tbl_t* tblCfg;
	pgIdx_t idxTop;

	_ASSERT_DEBUG(edbHndl->tables.length == 0);

	idxTop = edb_getResourcePgIdx(edbHndl, FPM_R_TABLE_DEFS);
	if (idxTop == 0) {
		return EDB_NOT_FOUND;
	}

	err = edb_obj_open(edbHndl, &o, idxTop);

	if (!err) {
		while (!(err = edb_obj_read(&o, (void*) buff, sizeof(EDB_TblCfg_t)))) {

			tblCfg = _OS_MAlloc(sizeof(edb_tbl_t));
			if (tblCfg == NULL)
				return EDB_PM_OUT_OF_MEMORY;

			memset(tblCfg, 0, sizeof(edb_tbl_t));

			memcpy(&tblCfg->c, buff, sizeof(EDB_TblCfg_t));

			tblCfg->edbHndl = edbHndl;

			dll_init(&tblCfg->indexs, 16);
			dll_add(&edbHndl->tables, &tblCfg->tables);
		};
		edb_obj_close(&o);

		if (err == EDB_PM_EXCEEDS_EOF_VALUE)
			err = EDB_OKAY;
	}

	return err;
}

EDB_result_t EDB_tableCount(EDB_xHndl_t x, edb_tblHndl_t tbl, uint32_t* out_count) {
	EDB_result_t r;

	if ((r = edb_preOp(x)))
		return r;

	r = (EDB_result_t) BPT_count(&tbl->rowIndex->bptContext, out_count);

	edb_postOp(x);
	return r;
}

//void edb_cleanOrphanedIndexElements(edb_tblHndl_t tbl) {
//	uint32_t maxIndexSize = 0;
//	uint32_t minIndexSize = 0;
//	uint32_t ct;
//	uint32_t ws = NULL;
//	edb_tblHndl_t tblHndl;
//
//	edb_tblIdxHndl_t IdxHndl;
//	BPT_Result_e r;
//
//	FOR_EACH_dll_ELEMENT2(&(tbl->indexs), el)
//	{
//		IdxHndl = (edb_tblIdxHndl_t) el;
//		edb_idxCount(IdxHndl, &ct);
//		if (ct > maxIndexSize)
//			maxIndexSize = ct;
//		if(minIndexSize = 0 || ct < minIndexSize)
//			minIndexSize = ct;
//	}
//
//	if(minIndexSize < maxIndexSize){
//		ws = SYS_MAlloc(maxIndexSize * 2);
//
//		FOR_EACH_dll_ELEMENT2(&(tbl->indexs), el)
//		{
//			IdxHndl = (edb_tblIdxHndl_t) el;
//			edb_idxCount(IdxHndl, &ct);
//			if(ct > minIndexSize){
////				BPT_cursor_t cur;
////				BPT_record_t record;
////				uint32_t minKey = 0, minId = 0, maxKey = 0, maxId = 0;
////
////				int ct = 0;
////
////				r = BPT_initSearch_All(&IdxHndl->bptContext, &cur);
////
////				while (BPT_findNext(&cur, &record) == BPT_CS_CURSOR_ACTIVE) {
////					if (ct == 0) {
////						minKey = record.key;
////						minId = record.valuePgIdx;
////					} else {
////						maxKey = record.key;
////						maxId = record.valuePgIdx;
////					}
////					ct++;
////				}
//			}
//		}
//
//	}
//	//maxIndexSize
//
//	done:
//	if(ws != NULL)
//		SYS_Free(ws);
//}

static void edb_checkTbl(edb_tblHndl_t tbl);

void EDB_checkTbl(EDB_xHndl_t x, edb_tblHndl_t tbl) {
	edb_preOp(x);
	edb_checkTbl(tbl);
	edb_postOp(x);
}
void edb_checkTbl(edb_tblHndl_t tbl) {
	EDB_PRINT_I("Tbl: %d) flags %d ", tbl->c.id, tbl->c.flags);

	edb_checkIndexs(tbl);
}

void EDB_checkTbls(EDB_xHndl_t x) {
	edb_tblHndl_t tblHndl;
	EDB_Hndl_t edbHndl;

	edb_preOp(x);

	edbHndl = x->edbHndl;

	EDB_PRINT_I("Table CT %d", edbHndl->tables.length);
	FOR_EACH_dll_ELEMENT2(&(edbHndl->tables), el)
	{
		tblHndl = (edb_tblHndl_t) el;
		edb_checkTbl(tblHndl);

	}
	edb_postOp(x);
}

#endif
#endif
