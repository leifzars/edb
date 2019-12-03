/*
 * table.h
 *
 */

#ifndef EDB_TABLE_TABLE_H
#define EDB_TABLE_TABLE_H

#include <edb/config.h>
#if INCLUDE_EDB
#if EDB_ENABLE_X

#define EDB_KEY_INDEX_ID			1

#include "../index/index.h"
typedef uint32_t EDB_tblID_t;

typedef struct {
	//pgIdx_t ownerPgIdx;
	EDB_IdxID_t id;

	EDB_TblIdxCfg_t rowIndex;

	pgIdx_t indexsPgIdx;

	uint32_t flags;

} EDB_TblCfg_t;
typedef EDB_TblCfg_t* EDB_TblCfgHndl_t;

typedef struct edb_tbl_t{
	dllElement_t tables;
	struct EDB_struct_t* edbHndl;
	EDB_TblCfg_t c;
	pgIdx_t tableDefPgIdx;

	edb_tblIdxHndl_t rowIndex;
	doubleLinkList_t indexs;

} edb_tbl_t;
typedef edb_tbl_t* edb_tblHndl_t;

EDB_result_t EDB_tableGetOrCreate(EDB_xHndl_t x, EDB_tblID_t id, edb_tblHndl_t* out_tblHndl);
EDB_result_t EDB_tableAdd(EDB_xHndl_t x, EDB_tblID_t id, edb_tblHndl_t* out_tblHndl);
EDB_result_t EDB_tableGet(EDB_xHndl_t x, EDB_tblID_t id, edb_tblHndl_t* out_tblHndl);

EDB_result_t EDB_tableCount(EDB_xHndl_t x, edb_tblHndl_t tbl, uint32_t* out_count);

void EDB_checkTbl(EDB_xHndl_t x, edb_tblHndl_t tbl);
void EDB_checkTbls(EDB_xHndl_t x);

#endif
#endif
#endif /* EDB_TABLE_TABLE_H */
