/*
 * error.c
 *
 */


#include "error.h"

EDB_result_t edb_handleIOError(EDB_Hndl_t pgMgrHndl, int err) {
	pgMgrHndl->lastIOError = err;
	pgMgrHndl->isInBadSate = true;
	return EDB_PM_IO_ERROR;
}

EDB_result_t edb_handleError(EDB_Hndl_t pgMgrHndl, EDB_result_t err) {
	pgMgrHndl->isInBadSate = true;
	return err;
}
