/*
 * transaction_prv.h
 *
 */

#ifndef EDB_TRANSACTION_PRV_H
#define EDB_TRANSACTION_PRV_H

#include <edb/config.h>
#if INCLUDE_EDB
#if EDB_ENABLE_X
#include "transaction.h"

EDB_result_t edb_trns_init(EDB_Hndl_t edbHndl);
void edb_trns_deinit(EDB_Hndl_t edbHndl);


void edb_trns_addPg(EDB_xHndl_t x, pgIdx_t pgIdx, uint32_t walOffset);
void edb_trns_addPgToVec(vec_t * vec, pgIdx_t pgIdx, uint32_t walOffset);

EDB_result_t edb_trns_start(EDB_Hndl_t edb, uint32_t xId, EDB_xHndl_t* out_newXHndl, EDB_trnsType_t type);
EDB_result_t edb_trns_commitMetaData(EDB_xHndl_t trnsHndl);
EDB_result_t edb_trns_cancelMetaData(EDB_xHndl_t xHndl);

void edb_trns_makeWriter(EDB_xHndl_t x);

EDB_result_t edb_trns_fail(EDB_Hndl_t edbHndl, EDB_xHndl_t trnsHndl);


void edb_trns__cacheSync(EDB_Hndl_t edbHndl);

static inline void edb_trns_enter(EDB_xHndl_t xHndl) {
	EDB_Hndl_t edbHndl = (EDB_Hndl_t) xHndl->edbHndl;

#if EDB_ENABLED_NESTED_X
#if EDB_ENABLED_NESTED_WRITE_X
	_ASSERT_DEBUG(xHndl->child == NULL);
#endif

	_ASSERT_DEBUG(edbHndl->x.active == NULL);

	if (xHndl->dataVersion != edbHndl->x.prev.dataVersion) {
		edb_trns__cacheSync(edbHndl);
	}
#endif

	edbHndl->x.active = xHndl;
}

static inline void edb_trns_exit(EDB_xHndl_t xHndl) {
	EDB_Hndl_t edbHndl = (EDB_Hndl_t) xHndl->edbHndl;

	_ASSERT_DEBUG(edbHndl->x.active == xHndl);

	edbHndl->x.active = NULL;

#if EDB_ENABLED_NESTED_X
#if EDB_ENABLED_NESTED_WRITE_X
	_ASSERT_DEBUG(xHndl->child == NULL);
#endif
	edbHndl->x.prev.dataVersion = xHndl->dataVersion;
#endif
}

EDB_result_t edb_trns_failed(EDB_xHndl_t xHndl);

#endif
#endif
#endif /* EDB_TRANSACTION_PRV_H */
