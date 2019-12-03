/*
 * walIndex.h
 *
 */

#ifndef EDB_WAL_WALINDEX_H
#define EDB_WAL_WALINDEX_H

#include <edb/config.h>
#if INCLUDE_EDB
#if EDB_ENABLE_X

#include <edb/pageMgr/pm.h>

void edb_walIdx_init(EDB_Hndl_t edbHndl);
void edb_walIdx_deinit(EDB_Hndl_t edbHndl);

EDB_result_t edb_walIdx_find(EDB_Hndl_t edbHndl, pgIdx_t pgIdx, uint32_t* out_walOffset);
EDB_result_t edb_walIdx_addPageToX(EDB_Hndl_t edbHndl, pgIdx_t pgIdx, uint32_t walOffset);
void edb_walIdx_appendXModedPages(EDB_Hndl_t edbHndl, EDB_xHndl_t x);

EDB_result_t edb_walIdx_startX(EDB_Hndl_t edbHndl);

void edb_walIdx_purge(EDB_Hndl_t edbHndl);
void edb_walIdx_remove(EDB_Hndl_t edbHndl, uint32_t itr);
uint32_t edb_walIdx_size(EDB_Hndl_t edbHndl);

void edb_walIdx_start(EDB_Hndl_t edbHndl, uint32_t id);
void edb_walIdx_end(EDB_Hndl_t edbHndl, uint32_t id);
EDB_result_t edb_walIdx_addEntry(EDB_Hndl_t edbHndl, FPM_WAL_entry_t* entry, uint32_t walOffset);

#endif
#endif
#endif /* EDB_WAL_WALINDEX_H */
