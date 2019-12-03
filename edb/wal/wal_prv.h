/*
 * wal_prv.h
 *
 */

#ifndef EDB_WAL_WAL_PRV_H
#define EDB_WAL_WAL_PRV_H

#include <edb/config.h>
#if INCLUDE_EDB
#if EDB_ENABLE_X

#include <edb/pageMgr/pm_prv.h>

EDB_result_t edb_wal_io_flush(EDB_Hndl_t edbHndl);
void edb_wal_io_resetCache(EDB_Hndl_t edbHndl);
EDB_result_t edb_wal_io_write(EDB_Hndl_t edbHndl, void* buf, uint32_t len);
EDB_result_t edb_wal_read(EDB_Hndl_t edbHndl, uint32_t walOffset, void* buf, uint32_t len);

#endif
#endif
#endif /* EDB_WAL_WAL_PRV_H */
