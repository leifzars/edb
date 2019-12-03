/*
 * index_prv.h
 *
 */

#ifndef EDB_INDEX_INDEX_PRV_H
#define EDB_INDEX_INDEX_PRV_H

#include <edb/config.h>
#if INCLUDE_EDB
#if EDB_ENABLE_X

#include "index.h"

#include "lib/doubleLinkList.h"


EDB_result_t edb_indexs_load(EDB_Hndl_t edbHndl, struct edb_tbl_t* tbl);
void edb_indexs_unload(struct edb_tbl_t* tbl);

edb_tblIdxHndl_t edb_getIndex(struct edb_tbl_t* tbl, EDB_IdxID_t id) ;
EDB_result_t edb_addIndex(EDB_Hndl_t edbHndl, struct edb_tbl_t* tbl, EDB_IdxID_t id, bool allowDupKeys, edb_tblIdxHndl_t* out_idxHdnl) ;

EDB_result_t edb_indexs_flush(EDB_Hndl_t edbHndl, struct edb_tbl_t* tbl, bool andInvalidate);
void edb_indexs_cacheInvalidate(EDB_Hndl_t edbHndl, struct edb_tbl_t* tbl);


EDB_result_t edb_idxCount(edb_tblIdxHndl_t idx, uint32_t* out_count);

#endif
#endif
#endif /* EDB_INDEX_INDEX_PRV_H */
