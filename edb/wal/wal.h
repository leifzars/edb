/*
 * wal.h
 *
 */

#ifndef EDB_WAL_WAL_H
#define EDB_WAL_WAL_H



#include <edb/config.h>
#if INCLUDE_EDB
#if EDB_ENABLE_X

#include "../pageMgr/pm.h"
//#include "wal/walIndex.h"

EDB_result_t edb_wal_init(EDB_Hndl_t edbHndl) ;
void edb_wal_deinit(EDB_Hndl_t edbHndl);
//EDB_result_t edb_wal_readPage(FPM_Hndl_t edbHndl, EDB_xHndl_t xHndl);

//EDB_result_t edb_wal_wrtPage(FPM_Hndl_t edbHndl, EDB_xHndl_t xHndl);
EDB_result_t edb_wal_wrtXStart(EDB_Hndl_t edbHndl, EDB_xHndl_t xHndl);
EDB_result_t edb_wal_wrtXCommit(EDB_Hndl_t edbHndl, EDB_xHndl_t xHndl);
EDB_result_t edb_wal_wrtXCancle(EDB_Hndl_t edbHndl, EDB_xHndl_t xHndl);

uint32_t edb_wal_percentFull(EDB_Hndl_t edbHndl);
EDB_result_t edb_wal_sync(EDB_Hndl_t edbHndl) ;
void edb_wal_evaluateForSync(EDB_Hndl_t edbHndl);

//sb priv
EDB_result_t edb_wal_writePage_fromFPM(EDB_Hndl_t edbHndl, edb_pm_pgBufHndl_t pgBuf);
EDB_result_t edb_wal_readPage_fromFPM(EDB_Hndl_t edbHndl, pgIdx_t pgIdx, void* buf);
EDB_result_t edb_wal_writePages_fromFPM(EDB_Hndl_t edbHndl, uint32_t num, edb_pm_pgBufHndl_t* bufs);

EDB_result_t edb_wal_appendEntry(EDB_Hndl_t edbHndl, FPM_WAL_entry_t* entry);


#endif
#endif
#endif /* EDB_WAL_WAL_H */
