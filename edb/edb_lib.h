/*
 * edb_lib.h
 *
 */

#ifndef EDB_EDB_LIB_H
#define EDB_EDB_LIB_H

#include "config.h"
#if INCLUDE_EDB

#include <edb/pageMgr/pm.h>
#include "lib/doubleLinkList.h"

#include "pageMgr/pm_s.h"
#include "wal/walTypes.h"

#include "table/table.h"
#include "index/index.h"

#include "transaction/transaction.h"
#include "transaction/transactionTypes.h"
//#include "lib/filePageManager/bTree/bpt_fpm.h"


void EDB_register();
EDB_result_t EDB_open(EDB_Hndl_t* edbHndl, edb_pm_openArgsPtr_t fpmOpenArgs);
void EDB_close(EDB_Hndl_t edbHndl);
EDB_result_t EDB_flush(EDB_Hndl_t edbHndl);

#if EDB_ENABLE_X
EDB_result_t EDB_set(EDB_xHndl_t x, edb_tblHndl_t tbl, kvp_key_t key, buf_t* value);
EDB_result_t EDB_append(EDB_xHndl_t x,edb_tblHndl_t tb, kvp_key_t key, buf_t* value);
EDB_result_t EDB_get(EDB_xHndl_t x,edb_tblHndl_t tb, kvp_key_t key, buf_t* value);
EDB_result_t EDB_get_byValuePgIdx(EDB_xHndl_t x,edb_tblHndl_t tb, pgIdx_t valuePageIdx, buf_t* value);
EDB_result_t edb_get_byValuePgIdx(EDB_xHndl_t x, edb_tblHndl_t tbl, pgIdx_t valuePageIdx, buf_t* value);
EDB_result_t EDB_getPage(EDB_xHndl_t x,edb_tblHndl_t tb, kvp_key_t key, buf_t* value);
EDB_result_t EDB_delete(EDB_xHndl_t x,edb_tblHndl_t tb, kvp_key_t key);
EDB_result_t EDB_exists(EDB_xHndl_t x,edb_tblHndl_t tb, kvp_key_t key);


EDB_result_t EDB_openValue(EDB_xHndl_t x,edb_tblHndl_t tb, kvp_key_t key, FPM_Object_t* valueHndl);

EDB_result_t EDB_openValue_ByPage(EDB_xHndl_t x, FPM_Object_t* valueHndl, pgIdx_t pgIdx);
#endif

void EDB_check(EDB_Hndl_t edbHndl);
void EDB_test();
#endif
#endif /* EDB_EDB_LIB_H */
