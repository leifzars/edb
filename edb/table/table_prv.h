/*
 * table_prv.h
 *
 */

#ifndef SOURCES_LIB_FILEPAGEMANAGER_TABLE_edb_TABLE_PRV_H_
#define SOURCES_LIB_FILEPAGEMANAGER_TABLE_edb_TABLE_PRV_H_

#include <edb/config.h>
#if INCLUDE_EDB
#if EDB_ENABLE_X

#include "table.h"


edb_tblHndl_t EDB_getTable(EDB_Hndl_t edbHndl, EDB_tblID_t id);

EDB_result_t edb_tables_load(EDB_Hndl_t edbHndl);
void edb_tables_unload(EDB_Hndl_t edbHndl);
EDB_result_t edb_tables_cacheFlush(EDB_Hndl_t edbHndl, bool andInvalidate);
void edb_tables_cacheInvalidate(EDB_Hndl_t edbHndl);

#endif
#endif
#endif /* SOURCES_LIB_FILEPAGEMANAGER_TABLE_edb_TABLE_PRV_H_ */
