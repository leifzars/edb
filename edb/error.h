/*
 * error.h
 *
 */

#ifndef EDB_ERROR_H_
#define EDB_ERROR_H_

#include "pageMgr/pm_s.h"
#include "pageMgr/pm_types.h"

EDB_result_t edb_handleError(EDB_Hndl_t pgMgrHndl, EDB_result_t err);
EDB_result_t edb_handleIOError(EDB_Hndl_t pgMgrHndl, int err);

#endif /* EDB_ERROR_H_ */
