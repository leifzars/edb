/*
 * trasaction.h
 *
 */

#ifndef EDB_TRANSACTION_H
#define EDB_TRANSACTION_H

#include <edb/config.h>

#if INCLUDE_EDB
#if EDB_ENABLE_X

typedef enum{
	EDB_X_ANY,
	EDB_X_READ_ONLY,
	EDB_X_WRITER,
}EDB_trnsType_t;


EDB_result_t EDB_trns_start(EDB_Hndl_t edbHndl, EDB_xHndl_t* out_newTrns, EDB_trnsType_t type);

EDB_result_t EDB_trns_commit(EDB_xHndl_t trnsHndl);


EDB_result_t EDB_trns_cancel(EDB_xHndl_t trnsHndl);

uint32_t EDB_trns_percentSpaceRemaing(EDB_Hndl_t edbHndl, EDB_xHndl_t trnsHndl);




#endif
#endif
#endif /* EDB_TRANSACTION_H */
