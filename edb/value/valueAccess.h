/*
 * valueAccess.h
 *
 */

#ifndef EDB_VALUE_VALUEACCESS_H
#define EDB_VALUE_VALUEACCESS_H

#include <edb/config.h>
#if INCLUDE_EDB

#include <edb/pageMgr/pm_prv.h>

EDB_result_t edb_val_add(EDB_Hndl_t pgMgrHndl, uint32_t key, pgIdx_t* out_valuePgIdx);

EDB_result_t FPM_insertValue(EDB_Hndl_t pgMgrHndl, uint32_t key, void* value, uint32_t valueLen, pgIdx_t* out_pgIdx);
EDB_result_t FPM_updateValue(EDB_Hndl_t pgMgrHndl, pgIdx_t currentPgIdx, uint32_t key, void* value, uint32_t valueLen, pgIdx_t* out_newPgIdx);
EDB_result_t FPM_appendValue(EDB_Hndl_t pgMgrHndl, pgIdx_t valuePgIdx, uint32_t key, void* value, uint32_t valueLen);

EDB_result_t FPM_vm_setValue(EDB_Hndl_t pgMgrHndl, pgIdx_t masterPgIdx, uint32_t key, void* value, uint32_t valueLen);

EDB_result_t FPM_deleteValue(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx);


EDB_result_t FPM_getValueSize(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx, uint32_t* out_size);
EDB_result_t FPM_getValueData(EDB_Hndl_t pgMgrHndl, pgIdx_t pgIdx, void** dataBufferPtr);


typedef struct {
	void* pageBuffer;
	void* valueBuffer;
	kvp_key_t key;
	pgIdx_t pgIdx;
	uint32_t valueSize;
} EDB_valueAccessInfo_t;

EDB_result_t FPM_getValuePage(EDB_Hndl_t pgMgrHndl, EDB_valueAccessInfo_t* accessInfo);
void FPM_returnValuePage(EDB_Hndl_t pgMgrHndl,  void* value);

#endif
#endif /* EDB_VALUE_VALUEACCESS_H */
