/*
 * index.h
 *
 */

#ifndef EDB_INDEX_INDEX_H
#define EDB_INDEX_INDEX_H

#include <edb/config.h>
#if INCLUDE_EDB
#if EDB_ENABLE_X


#include "lib/doubleLinkList.h"

#include <edb/bTree/cursor.h>
#include "../table/table.h"


typedef uint32_t EDB_IdxID_t;

typedef struct {
	//pgIdx_t ownerPgIdx;
	EDB_IdxID_t id;

	uint32_t isPageIdx :1;
	uint32_t allowDupKeys :1;

	uint32_t usedPageCount;

	pgIdx_t rootPageIdxs[BPT_ROOT_SIZE_NUM_OF_SECTORS];
} EDB_TblIdxCfg_t;
typedef EDB_TblIdxCfg_t* EDB_TblIdxCfgHndl_t;

typedef struct edb_tblIdxHndl{
	dllElement_t indexs;
	struct edb_tbl_t* tbl;
	EDB_TblIdxCfg_t c;

	BPT_t bptContext;

} edb_tblIdx_t;
typedef edb_tblIdx_t* edb_tblIdxHndl_t;

EDB_result_t EDB_tblIdxGet(EDB_xHndl_t x, struct  edb_tbl_t* tbl, EDB_IdxID_t id, edb_tblIdxHndl_t* out_idx);
EDB_result_t EDB_tblIdxAdd(EDB_xHndl_t x, struct  edb_tbl_t* tbl, EDB_IdxID_t id, bool allowDupKeys,
		edb_tblIdxHndl_t* out_idx);

EDB_result_t EDB_idxAddKey(EDB_xHndl_t x, edb_tblIdxHndl_t idx, void *key, bpt_exAddress_t rec);
EDB_result_t EDB_idxDeleteKey(EDB_xHndl_t x, edb_tblIdxHndl_t idx, void *key, bpt_exAddress_t *rec);
EDB_result_t EDB_idxGetValue(EDB_xHndl_t x, edb_tblIdxHndl_t idx, uint32_t key, pgIdx_t* val);
bool EDB_idxExists_match(EDB_xHndl_t x, edb_tblIdxHndl_t idx, uint32_t key, pgIdx_t val);


BPT_Result_e EDB_idx_initSearch_All(EDB_xHndl_t x, edb_tblIdxHndl_t idx, BPT_crsHndl_t cursor, BPT_order_t order);
BPT_Result_e EDB_idx_initSearch_Range(EDB_xHndl_t x, edb_tblIdxHndl_t idx, BPT_crsHndl_t cursor, kvp_key_t lower,
		kvp_key_t upper, BPT_order_t order);
BPT_Result_e EDB_idx_initSearch_Equal(EDB_xHndl_t x, edb_tblIdxHndl_t idx, BPT_crsHndl_t cursor, kvp_key_t value);
BPT_cursorStatus_t EDB_idx_findNext(EDB_xHndl_t x, BPT_crsHndl_t cursor, BPT_record_t *out_record);
//EDB_result_t EDB_idxGetValue(edb_tblIdxHndl_t idx, uint32_t key, pgIdx_t* val);

EDB_result_t EDB_idxCount(EDB_xHndl_t x, edb_tblIdxHndl_t idx, uint32_t* out_count);

BPT_cursorStatus_t EDB_idx_prevPeriod(EDB_xHndl_t x, BPT_hndl_t handle, BPT_crsHndl_t cursor, kvp_key_t validLow,
		kvp_key_t validHigh);

void edb_checkIndexs(struct  edb_tbl_t* tbl);

#endif
#endif
#endif /* EDB_INDEX_INDEX_H */
